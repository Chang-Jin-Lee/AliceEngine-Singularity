// SPDX-License-Identifier: MIT
#include "Profiler.h"

#include "Log.h"
#include "StringUtil.h"
#include "Time.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace alice {
namespace {

constexpr const char* kChannel = "perf";

/// 스레드 하나의 존 버퍼. 락 없이 쓰고, 프레임 끝에서만 수집한다.
struct ThreadBuffer {
    std::vector<ZoneRecord> zones;
    std::vector<i32>        openStack;
    u32                     threadId = 0;
    std::atomic<bool>       alive{true};
};

struct GpuZone {
    std::string name;
    u64         durationNs = 0;
};

struct ProfilerState {
    std::atomic<bool> enabled{true};

    std::mutex                                 bufferMutex;
    std::vector<std::shared_ptr<ThreadBuffer>> buffers;

    std::mutex            frameMutex;
    std::deque<FrameData> history;
    u32                   historyLimit = 240;

    u64  currentFrame   = 0;
    u64  currentBeginNs = 0;
    bool frameOpen      = false;

    std::mutex                                  gpuMutex;
    std::map<u64, std::vector<GpuZone>>         gpuPending;

    std::mutex                                  budgetMutex;
    std::map<std::string, f64, std::less<>>     budgetsMs;
    std::atomic<u64>                            violations{0};
};

ProfilerState& State() {
    static ProfilerState s;
    return s;
}

u32 NextThreadOrdinal() noexcept {
    static std::atomic<u32> counter{1};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

/// 스레드마다 하나. 소멸 시 스스로 등록을 해제한다.
struct ThreadBufferHandle {
    std::shared_ptr<ThreadBuffer> buffer;

    ThreadBufferHandle() {
        buffer = std::make_shared<ThreadBuffer>();
        buffer->threadId = NextThreadOrdinal();
        buffer->zones.reserve(1024);
        ProfilerState& s = State();
        std::lock_guard<std::mutex> lock(s.bufferMutex);
        s.buffers.push_back(buffer);
    }
    ~ThreadBufferHandle() {
        if (buffer) buffer->alive.store(false, std::memory_order_relaxed);
    }
};

ThreadBuffer* LocalBuffer() {
    thread_local ThreadBufferHandle handle;
    return handle.buffer.get();
}

/// 이름별로 접는다. selfNs = totalNs - (직계 자식 totalNs 합).
std::vector<ZoneAggregate> FoldZones(const std::vector<const FrameData*>& frames) {
    std::unordered_map<std::string, ZoneAggregate> byName;

    for (const FrameData* frame : frames) {
        const std::vector<ZoneRecord>& zones = frame->zones;

        // 직계 자식 시간 합계를 먼저 구한다.
        std::vector<u64> childTotal(zones.size(), 0);
        for (usize i = 0; i < zones.size(); ++i) {
            const ZoneRecord& z = zones[i];
            if (z.parent >= 0 && static_cast<usize>(z.parent) < zones.size()) {
                childTotal[static_cast<usize>(z.parent)] += (z.endNs - z.beginNs);
            }
        }

        for (usize i = 0; i < zones.size(); ++i) {
            const ZoneRecord& z = zones[i];
            const u64 total = (z.endNs > z.beginNs) ? (z.endNs - z.beginNs) : 0;
            const u64 self  = (total > childTotal[i]) ? (total - childTotal[i]) : 0;

            ZoneAggregate& agg = byName[z.name];
            if (agg.name.empty()) agg.name = z.name;
            agg.totalNs += total;
            agg.selfNs  += self;
            agg.maxNs    = std::max(agg.maxNs, total);
            agg.calls   += 1;
        }
    }

    std::vector<ZoneAggregate> out;
    out.reserve(byName.size());
    for (auto& kv : byName) out.push_back(std::move(kv.second));
    std::sort(out.begin(), out.end(), [](const ZoneAggregate& a, const ZoneAggregate& b) {
        if (a.selfNs != b.selfNs) return a.selfNs > b.selfNs;
        return a.name < b.name;
    });
    return out;
}

f64 Percentile(std::vector<f64>& sorted, f64 q) {
    if (sorted.empty()) return 0.0;
    const usize idx = static_cast<usize>(q * static_cast<f64>(sorted.size() - 1) + 0.5);
    return sorted[std::min(idx, sorted.size() - 1)];
}

} // namespace

// ── 켜고 끄기 ──────────────────────────────────────────────────────────────
void Profiler::SetEnabled(bool enabled) noexcept { State().enabled.store(enabled, std::memory_order_relaxed); }
bool Profiler::IsEnabled() noexcept { return State().enabled.load(std::memory_order_relaxed); }

void Profiler::SetHistoryFrames(u32 count) {
    ProfilerState& s = State();
    std::lock_guard<std::mutex> lock(s.frameMutex);
    s.historyLimit = count ? count : 1;
    while (s.history.size() > s.historyLimit) s.history.pop_front();
}

// ── 존 ─────────────────────────────────────────────────────────────────────
void Profiler::BeginZone(const char* name) noexcept {
    if (!IsEnabled()) return;
    ThreadBuffer* buf = LocalBuffer();

    ZoneRecord z;
    z.name     = name ? name : "?";
    z.beginNs  = SteadyNanos();
    z.depth    = static_cast<u32>(buf->openStack.size());
    z.parent   = buf->openStack.empty() ? -1 : buf->openStack.back();
    z.threadId = buf->threadId;

    buf->zones.push_back(z);
    buf->openStack.push_back(static_cast<i32>(buf->zones.size()) - 1);
}

void Profiler::EndZone() noexcept {
    if (!IsEnabled()) return;
    ThreadBuffer* buf = LocalBuffer();
    if (buf->openStack.empty()) return;   // BeginFrame 이 중간에 버퍼를 비운 경우

    const i32 idx = buf->openStack.back();
    buf->openStack.pop_back();
    if (idx >= 0 && static_cast<usize>(idx) < buf->zones.size()) {
        buf->zones[static_cast<usize>(idx)].endNs = SteadyNanos();
    }
}

// ── 프레임 ─────────────────────────────────────────────────────────────────
void Profiler::BeginFrame(u64 frameIndex) {
    ProfilerState& s = State();
    if (!IsEnabled()) return;

    {
        std::lock_guard<std::mutex> lock(s.bufferMutex);
        // 죽은 스레드 버퍼를 정리하고 살아있는 것을 비운다.
        s.buffers.erase(
            std::remove_if(s.buffers.begin(), s.buffers.end(),
                           [](const std::shared_ptr<ThreadBuffer>& b) {
                               return !b->alive.load(std::memory_order_relaxed);
                           }),
            s.buffers.end());
        for (auto& b : s.buffers) {
            b->zones.clear();
            b->openStack.clear();
        }
    }

    std::lock_guard<std::mutex> lock(s.frameMutex);
    s.currentFrame   = frameIndex;
    s.currentBeginNs = SteadyNanos();
    s.frameOpen      = true;
}

void Profiler::EndFrame() {
    ProfilerState& s = State();
    if (!IsEnabled()) return;

    const u64 endNs = SteadyNanos();

    FrameData frame;
    {
        std::lock_guard<std::mutex> lock(s.frameMutex);
        if (!s.frameOpen) return;
        s.frameOpen      = false;
        frame.frameIndex = s.currentFrame;
        frame.beginNs    = s.currentBeginNs;
        frame.endNs      = endNs;
    }

    // 스레드 버퍼를 이어 붙인다. parent 인덱스는 버퍼 로컬이므로 오프셋을 더해 재배치한다.
    {
        std::lock_guard<std::mutex> lock(s.bufferMutex);
        for (auto& b : s.buffers) {
            const usize base = frame.zones.size();
            for (ZoneRecord z : b->zones) {
                if (z.endNs == 0) z.endNs = endNs;   // 프레임 경계에서 닫히지 않은 존
                if (z.parent >= 0) z.parent = static_cast<i32>(base) + z.parent;
                frame.zones.push_back(z);
            }
        }
    }

    // 이 프레임에 도착한 GPU 존을 합류시킨다(몇 프레임 늦게 온다).
    {
        std::lock_guard<std::mutex> lock(s.gpuMutex);
        auto it = s.gpuPending.find(frame.frameIndex);
        if (it != s.gpuPending.end()) {
            for (const GpuZone& g : it->second) {
                ZoneRecord z;
                z.name     = "gpu";   // 실제 이름은 별도 집계 경로에서 다룬다
                z.beginNs  = frame.beginNs;
                z.endNs    = frame.beginNs + g.durationNs;
                z.threadId = 0;
                frame.zones.push_back(z);
            }
            s.gpuPending.erase(it);
        }
        // 너무 오래된 미해결 GPU 존은 버린다.
        while (!s.gpuPending.empty() && s.gpuPending.begin()->first + 16 < frame.frameIndex) {
            s.gpuPending.erase(s.gpuPending.begin());
        }
    }

    // 예산 검사. 초과는 구조화 로그로 나간다 — 여기가 AI 진입점이다.
    {
        std::lock_guard<std::mutex> lock(s.budgetMutex);
        if (!s.budgetsMs.empty()) {
            const std::vector<const FrameData*> one{&frame};
            for (const ZoneAggregate& agg : FoldZones(one)) {
                auto it = s.budgetsMs.find(agg.name);
                if (it == s.budgetsMs.end()) continue;
                if (agg.TotalMs() <= it->second) continue;

                s.violations.fetch_add(1, std::memory_order_relaxed);
                ALICE_LOG_WARN(kChannel, "perf.budget.exceeded")
                    .Msg("존이 프레임 예산을 넘었다")
                    .F("zone", agg.name)
                    .F("actualMs", agg.TotalMs())
                    .F("budgetMs", it->second)
                    .F("overMs", agg.TotalMs() - it->second)
                    .F("calls", static_cast<i64>(agg.calls))
                    .F("frame", static_cast<i64>(frame.frameIndex));
            }
        }
    }

    std::lock_guard<std::mutex> lock(s.frameMutex);
    s.history.push_back(std::move(frame));
    while (s.history.size() > s.historyLimit) s.history.pop_front();
}

void Profiler::SubmitGpuZone(u64 frameIndex, const char* name, u64 durationNs) {
    ProfilerState& s = State();
    std::lock_guard<std::mutex> lock(s.gpuMutex);
    s.gpuPending[frameIndex].push_back(GpuZone{name ? name : "gpu", durationNs});
}

// ── 질의 ───────────────────────────────────────────────────────────────────
bool Profiler::LatestFrame(FrameData& out) {
    ProfilerState& s = State();
    std::lock_guard<std::mutex> lock(s.frameMutex);
    if (s.history.empty()) return false;
    out = s.history.back();
    return true;
}

bool Profiler::FrameByIndex(u64 frameIndex, FrameData& out) {
    ProfilerState& s = State();
    std::lock_guard<std::mutex> lock(s.frameMutex);
    for (auto it = s.history.rbegin(); it != s.history.rend(); ++it) {
        if (it->frameIndex == frameIndex) { out = *it; return true; }
    }
    return false;
}

std::vector<ZoneAggregate> Profiler::Aggregate(u32 frameCount) {
    ProfilerState& s = State();
    std::lock_guard<std::mutex> lock(s.frameMutex);
    if (s.history.empty()) return {};

    const usize n = std::min<usize>(frameCount ? frameCount : s.history.size(), s.history.size());
    std::vector<const FrameData*> frames;
    frames.reserve(n);
    for (usize i = s.history.size() - n; i < s.history.size(); ++i) {
        frames.push_back(&s.history[i]);
    }
    return FoldZones(frames);
}

FrameStats Profiler::Stats(u32 frameCount) {
    ProfilerState& s = State();
    std::vector<f64> ms;
    {
        std::lock_guard<std::mutex> lock(s.frameMutex);
        if (s.history.empty()) return {};
        const usize n = std::min<usize>(frameCount ? frameCount : s.history.size(), s.history.size());
        ms.reserve(n);
        for (usize i = s.history.size() - n; i < s.history.size(); ++i) {
            ms.push_back(static_cast<f64>(s.history[i].DurationNs()) * 1e-6);
        }
    }

    FrameStats st;
    st.sampleCount = static_cast<u32>(ms.size());
    f64 sum = 0.0;
    for (f64 v : ms) sum += v;
    st.avgMs = sum / static_cast<f64>(ms.size());

    std::sort(ms.begin(), ms.end());
    st.minMs = ms.front();
    st.maxMs = ms.back();
    st.p50Ms = Percentile(ms, 0.50);
    st.p95Ms = Percentile(ms, 0.95);
    st.p99Ms = Percentile(ms, 0.99);
    return st;
}

// ── 예산 ───────────────────────────────────────────────────────────────────
void Profiler::SetBudgetMs(std::string_view zoneName, f64 budgetMs) {
    ProfilerState& s = State();
    std::lock_guard<std::mutex> lock(s.budgetMutex);
    s.budgetsMs[std::string(zoneName)] = budgetMs;
}

void Profiler::ClearBudgets() {
    ProfilerState& s = State();
    std::lock_guard<std::mutex> lock(s.budgetMutex);
    s.budgetsMs.clear();
}

u64 Profiler::BudgetViolationCount() noexcept {
    return State().violations.load(std::memory_order_relaxed);
}

// ── 내보내기 ───────────────────────────────────────────────────────────────
bool Profiler::WriteChromeTrace(const std::string& path, u32 frameCount) {
    ProfilerState& s = State();

    std::vector<FrameData> frames;
    {
        std::lock_guard<std::mutex> lock(s.frameMutex);
        if (s.history.empty()) return false;
        const usize n = std::min<usize>(frameCount ? frameCount : s.history.size(), s.history.size());
        for (usize i = s.history.size() - n; i < s.history.size(); ++i) {
            frames.push_back(s.history[i]);
        }
    }

    std::FILE* file = nullptr;
#if ALICE_PLATFORM_WINDOWS
    fopen_s(&file, path.c_str(), "wb");
#else
    file = std::fopen(path.c_str(), "wb");
#endif
    if (!file) {
        ALICE_LOG_ERROR(kChannel, "perf.trace.write_failed")
            .Msg("트레이스 파일을 열지 못했다").F("path", path);
        return false;
    }

    // 시작점을 0 으로 옮긴다. 절대 시각은 뷰어에서 의미가 없다.
    const u64 origin = frames.front().beginNs;

    std::string out;
    out.reserve(1 << 16);
    out += "{\"displayTimeUnit\":\"ms\",\"traceEvents\":[";

    bool first = true;
    for (const FrameData& f : frames) {
        // 프레임 자체를 최상위 구간으로 하나 그린다.
        if (!first) out += ',';
        first = false;
        out += "{\"name\":\"Frame ";
        detail::FormatAppend(out, f.frameIndex);
        out += "\",\"cat\":\"frame\",\"ph\":\"X\",\"pid\":1,\"tid\":0,\"ts\":";
        out += FormatDouble(static_cast<f64>(f.beginNs - origin) / 1000.0);
        out += ",\"dur\":";
        out += FormatDouble(static_cast<f64>(f.DurationNs()) / 1000.0);
        out += '}';

        for (const ZoneRecord& z : f.zones) {
            out += ",{\"name\":";
            out += JsonQuote(z.name);
            out += ",\"cat\":\"cpu\",\"ph\":\"X\",\"pid\":1,\"tid\":";
            detail::FormatAppend(out, z.threadId);
            out += ",\"ts\":";
            out += FormatDouble(static_cast<f64>(z.beginNs - origin) / 1000.0);
            out += ",\"dur\":";
            out += FormatDouble(static_cast<f64>(z.endNs - z.beginNs) / 1000.0);
            out += '}';
        }
    }
    out += "]}";

    std::fwrite(out.data(), 1, out.size(), file);
    std::fclose(file);

    ALICE_LOG_INFO(kChannel, "perf.trace.written")
        .Msg("크롬 트레이스를 저장했다")
        .F("path", path)
        .F("frames", static_cast<i64>(frames.size()))
        .F("bytes", static_cast<i64>(out.size()));
    return true;
}

std::string Profiler::HotspotsJson(u32 topN, u32 frameCount) {
    const std::vector<ZoneAggregate> agg = Aggregate(frameCount);
    const FrameStats st = Stats(frameCount ? frameCount : 0);

    FrameData latest;
    const bool hasFrame = LatestFrame(latest);

    std::string out = "{\"frame\":";
    detail::FormatAppend(out, hasFrame ? latest.frameIndex : 0);
    out += ",\"frameMs\":";
    out += FormatDouble(hasFrame ? static_cast<f64>(latest.DurationNs()) * 1e-6 : 0.0);
    out += ",\"sampledFrames\":";
    detail::FormatAppend(out, st.sampleCount);
    out += ",\"stats\":{\"avgMs\":";
    out += FormatDouble(st.avgMs);
    out += ",\"minMs\":"; out += FormatDouble(st.minMs);
    out += ",\"maxMs\":"; out += FormatDouble(st.maxMs);
    out += ",\"p50Ms\":"; out += FormatDouble(st.p50Ms);
    out += ",\"p95Ms\":"; out += FormatDouble(st.p95Ms);
    out += ",\"p99Ms\":"; out += FormatDouble(st.p99Ms);
    out += "},\"budgetViolations\":";
    detail::FormatAppend(out, BudgetViolationCount());
    out += ",\"hotspots\":[";

    const usize n = std::min<usize>(topN, agg.size());
    for (usize i = 0; i < n; ++i) {
        if (i) out += ',';
        out += "{\"name\":";
        out += JsonQuote(agg[i].name);
        out += ",\"selfMs\":";  out += FormatDouble(agg[i].SelfMs());
        out += ",\"totalMs\":"; out += FormatDouble(agg[i].TotalMs());
        out += ",\"maxMs\":";   out += FormatDouble(static_cast<f64>(agg[i].maxNs) * 1e-6);
        out += ",\"calls\":";   detail::FormatAppend(out, agg[i].calls);
        out += '}';
    }
    out += "]}";
    return out;
}

void Profiler::Reset() {
    ProfilerState& s = State();
    {
        std::lock_guard<std::mutex> lock(s.bufferMutex);
        for (auto& b : s.buffers) { b->zones.clear(); b->openStack.clear(); }
    }
    {
        std::lock_guard<std::mutex> lock(s.frameMutex);
        s.history.clear();
        s.frameOpen = false;
    }
    {
        std::lock_guard<std::mutex> lock(s.gpuMutex);
        s.gpuPending.clear();
    }
    s.violations.store(0);
}

} // namespace alice
