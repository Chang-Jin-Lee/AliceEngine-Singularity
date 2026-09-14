// SPDX-License-Identifier: MIT
#include "Log.h"
#include "StringUtil.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <thread>

#if ALICE_PLATFORM_WINDOWS
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace alice {
namespace {

i64 NowUnixNanos() noexcept {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
}

u32 CurrentThreadId() noexcept {
#if ALICE_PLATFORM_WINDOWS
    return static_cast<u32>(::GetCurrentThreadId());
#else
    // 스레드 id 를 32비트로 접는다. 로그에서 스레드를 구분하는 용도로만 쓴다.
    static std::atomic<u32> counter{1};
    thread_local u32 id = counter.fetch_add(1, std::memory_order_relaxed);
    return id;
#endif
}

/// UNIX 나노초 → "HH:MM:SS.mmm" (지역시간). 콘솔 표시 전용.
std::string ClockText(i64 ns) {
    const std::time_t seconds = static_cast<std::time_t>(ns / 1000000000LL);
    const int millis = static_cast<int>((ns % 1000000000LL) / 1000000LL);
    std::tm tmv{};
#if ALICE_PLATFORM_WINDOWS
    localtime_s(&tmv, &seconds);
#else
    localtime_r(&seconds, &tmv);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec, millis);
    return buf;
}

const char* AnsiColorFor(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "\x1b[90m";   // 회색
        case LogLevel::Debug: return "\x1b[36m";   // 청록
        case LogLevel::Info:  return "\x1b[0m";
        case LogLevel::Warn:  return "\x1b[33m";   // 노랑
        case LogLevel::Error: return "\x1b[31m";   // 빨강
        case LogLevel::Fatal: return "\x1b[1;41m"; // 빨강 배경
        case LogLevel::Off:   return "\x1b[0m";
    }
    return "\x1b[0m";
}

char LevelChar(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return 'T';
        case LogLevel::Debug: return 'D';
        case LogLevel::Info:  return 'I';
        case LogLevel::Warn:  return 'W';
        case LogLevel::Error: return 'E';
        case LogLevel::Fatal: return 'F';
        case LogLevel::Off:   return '-';
    }
    return '?';
}

} // namespace

// ── LogLevel ───────────────────────────────────────────────────────────────
const char* ToString(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "trace";
        case LogLevel::Debug: return "debug";
        case LogLevel::Info:  return "info";
        case LogLevel::Warn:  return "warn";
        case LogLevel::Error: return "error";
        case LogLevel::Fatal: return "fatal";
        case LogLevel::Off:   return "off";
    }
    return "info";
}

bool ParseLogLevel(std::string_view s, LogLevel& out) noexcept {
    if (EqualsIgnoreCase(s, "trace")) { out = LogLevel::Trace; return true; }
    if (EqualsIgnoreCase(s, "debug")) { out = LogLevel::Debug; return true; }
    if (EqualsIgnoreCase(s, "info"))  { out = LogLevel::Info;  return true; }
    if (EqualsIgnoreCase(s, "warn") || EqualsIgnoreCase(s, "warning")) { out = LogLevel::Warn; return true; }
    if (EqualsIgnoreCase(s, "error")) { out = LogLevel::Error; return true; }
    if (EqualsIgnoreCase(s, "fatal")) { out = LogLevel::Fatal; return true; }
    if (EqualsIgnoreCase(s, "off"))   { out = LogLevel::Off;   return true; }
    return false;
}

// ── LogField ───────────────────────────────────────────────────────────────
std::string LogField::ValueToJson() const {
    switch (type) {
        case Type::String: return JsonQuote(str);
        case Type::Int:    { std::string o; detail::FormatAppend(o, i); return o; }
        case Type::Float:  return FormatDouble(f);
        case Type::Bool:   return b ? "true" : "false";
    }
    return "null";
}

std::string LogField::ValueToText() const {
    switch (type) {
        case Type::String: return str;
        case Type::Int:    { std::string o; detail::FormatAppend(o, i); return o; }
        case Type::Float:  return FormatDouble(f);
        case Type::Bool:   return b ? "true" : "false";
    }
    return "null";
}

// ── LogRecord ──────────────────────────────────────────────────────────────
std::string LogRecord::ToText() const {
    std::string out;
    out.reserve(160);
    out += ClockText(timestampNs);
    out += " [";
    out += LevelChar(level);
    out += "] ";

    // 채널을 고정폭으로 맞춰 눈으로 훑기 좋게 한다.
    out += channel;
    for (usize i = channel.size(); i < 10; ++i) out += ' ';
    out += ' ';

    out += event;
    if (!message.empty()) { out += "  "; out += message; }

    for (const LogField& f : fields) {
        out += "  ";
        out += f.name;
        out += '=';
        out += f.ValueToText();
    }
    return out;
}

std::string LogRecord::ToJson() const {
    std::string out;
    out.reserve(256);
    out += "{\"ts\":";
    detail::FormatAppend(out, timestampNs);
    out += ",\"lv\":";   out += JsonQuote(ToString(level));
    out += ",\"ch\":";   out += JsonQuote(channel);
    out += ",\"ev\":";   out += JsonQuote(event);
    out += ",\"frame\":"; detail::FormatAppend(out, frame);
    out += ",\"tid\":";   detail::FormatAppend(out, threadId);
    if (!message.empty()) { out += ",\"msg\":"; out += JsonQuote(message); }
    if (loc.file && loc.file[0]) {
        std::string src = ShortFile(loc.file);
        src += ':';
        detail::FormatAppend(src, loc.line);
        out += ",\"src\":";
        out += JsonQuote(src);
    }
    if (!fields.empty()) {
        out += ",\"f\":{";
        for (usize i = 0; i < fields.size(); ++i) {
            if (i) out += ',';
            out += JsonQuote(fields[i].name);
            out += ':';
            out += fields[i].ValueToJson();
        }
        out += '}';
    }
    out += '}';
    return out;
}

// ── 싱크 구현 ──────────────────────────────────────────────────────────────
namespace {

class ConsoleSink final : public ILogSink {
public:
    explicit ConsoleSink(bool color) : m_color(color) {
#if ALICE_PLATFORM_WINDOWS
        if (m_color) {
            // Windows 터미널에서 ANSI 를 켠다. 실패하면 색 없이 간다.
            HANDLE h = ::GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD mode = 0;
            if (h != INVALID_HANDLE_VALUE && ::GetConsoleMode(h, &mode)) {
                ::SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            } else {
                m_color = false;
            }
        }
#endif
    }

    void Write(const LogRecord& record) override {
        std::string line = record.ToText();
        std::lock_guard<std::mutex> lock(m_mutex);
        std::FILE* out = (record.level >= LogLevel::Error) ? stderr : stdout;
        if (m_color) {
            std::fputs(AnsiColorFor(record.level), out);
            std::fwrite(line.data(), 1, line.size(), out);
            std::fputs("\x1b[0m\n", out);
        } else {
            std::fwrite(line.data(), 1, line.size(), out);
            std::fputc('\n', out);
        }
        if (record.level >= LogLevel::Error) std::fflush(out);
    }

    void Flush() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::fflush(stdout);
        std::fflush(stderr);
    }

private:
    bool       m_color;
    std::mutex m_mutex;
};

class FileSink final : public ILogSink {
public:
    FileSink(const std::string& path, bool json) : m_json(json) {
#if ALICE_PLATFORM_WINDOWS
        fopen_s(&m_file, path.c_str(), "wb");
#else
        m_file = std::fopen(path.c_str(), "wb");
#endif
    }
    ~FileSink() override {
        if (m_file) { std::fflush(m_file); std::fclose(m_file); }
    }

    void Write(const LogRecord& record) override {
        if (!m_file) return;
        std::string line = m_json ? record.ToJson() : record.ToText();
        line += '\n';
        std::lock_guard<std::mutex> lock(m_mutex);
        std::fwrite(line.data(), 1, line.size(), m_file);
        // 에러 이상은 즉시 내린다. 크래시 직전 로그가 사라지면 원인 추적이 불가능하다.
        if (record.level >= LogLevel::Error) std::fflush(m_file);
    }

    void Flush() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file) std::fflush(m_file);
    }

private:
    std::FILE* m_file = nullptr;
    bool       m_json = false;
    std::mutex m_mutex;
};

} // namespace

std::shared_ptr<ILogSink> MakeConsoleSink(bool color) {
    return std::make_shared<ConsoleSink>(color);
}
std::shared_ptr<ILogSink> MakeTextFileSink(const std::string& path) {
    return std::make_shared<FileSink>(path, false);
}
std::shared_ptr<ILogSink> MakeJsonlFileSink(const std::string& path) {
    return std::make_shared<FileSink>(path, true);
}

// ── RingLogSink ────────────────────────────────────────────────────────────
struct RingLogSink::Impl {
    mutable std::mutex   mutex;
    std::deque<LogRecord> items;
    usize                 capacity = 8192;
};

RingLogSink::RingLogSink(usize capacity) : m_impl(std::make_shared<Impl>()) {
    m_impl->capacity = capacity ? capacity : 1;
}

void RingLogSink::Write(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->items.push_back(record);
    while (m_impl->items.size() > m_impl->capacity) m_impl->items.pop_front();
}

std::vector<LogRecord> RingLogSink::Tail(usize count) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    const usize n = std::min(count, m_impl->items.size());
    return std::vector<LogRecord>(m_impl->items.end() - static_cast<std::ptrdiff_t>(n),
                                  m_impl->items.end());
}

std::vector<LogRecord> RingLogSink::Query(LogLevel minimumLevel,
                                          std::string_view channel,
                                          std::string_view eventPrefix,
                                          usize limit) const {
    std::vector<LogRecord> out;
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    // 뒤에서부터 훑어 최신 limit 개를 모으고, 마지막에 시간순으로 되돌린다.
    for (auto it = m_impl->items.rbegin(); it != m_impl->items.rend(); ++it) {
        if (out.size() >= limit) break;
        if (it->level < minimumLevel) continue;
        if (!channel.empty() && it->channel != channel) continue;
        if (!eventPrefix.empty() && !StartsWith(it->event, eventPrefix)) continue;
        out.push_back(*it);
    }
    std::reverse(out.begin(), out.end());
    return out;
}

void RingLogSink::Clear() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->items.clear();
}

usize RingLogSink::Size() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->items.size();
}

// ── Log 파사드 ─────────────────────────────────────────────────────────────
namespace {

struct LogState {
    std::shared_mutex                       sinkMutex;
    std::vector<std::shared_ptr<ILogSink>>  sinks;

    std::shared_mutex                          channelMutex;
    // std::less<> 로 heterogeneous lookup 을 켠다 — string_view 조회에서 임시 string 이 생기지 않는다.
    std::map<std::string, LogLevel, std::less<>> channelLevels;

    std::atomic<LogLevel> globalLevel{LogLevel::Info};
    std::atomic<u64>      frame{0};
    std::atomic<u64>      counts[static_cast<usize>(LogLevel::Off) + 1]{};
    std::atomic<bool>     initialized{false};
};

LogState& State() {
    static LogState s;
    return s;
}

} // namespace

void Log::Initialize() {
    LogState& s = State();
    bool expected = false;
    if (!s.initialized.compare_exchange_strong(expected, true)) return;
    // 싱크는 호출자가 붙인다. 기본으로 콘솔 하나만 둔다 — 아무것도 안 보이는 것이 최악이다.
    AddSink(MakeConsoleSink(true));
}

void Log::Shutdown() {
    Flush();
    RemoveAllSinks();
    State().initialized.store(false);
}

void Log::AddSink(std::shared_ptr<ILogSink> sink) {
    if (!sink) return;
    LogState& s = State();
    std::unique_lock<std::shared_mutex> lock(s.sinkMutex);
    s.sinks.push_back(std::move(sink));
}

void Log::RemoveAllSinks() {
    LogState& s = State();
    std::unique_lock<std::shared_mutex> lock(s.sinkMutex);
    s.sinks.clear();
}

void Log::Flush() {
    LogState& s = State();
    std::shared_lock<std::shared_mutex> lock(s.sinkMutex);
    for (auto& sink : s.sinks) sink->Flush();
}

void Log::SetLevel(LogLevel level) noexcept { State().globalLevel.store(level, std::memory_order_relaxed); }
LogLevel Log::GetLevel() noexcept { return State().globalLevel.load(std::memory_order_relaxed); }

void Log::SetChannelLevel(std::string_view channel, LogLevel level) {
    LogState& s = State();
    std::unique_lock<std::shared_mutex> lock(s.channelMutex);
    s.channelLevels[std::string(channel)] = level;
}

void Log::ClearChannelLevels() {
    LogState& s = State();
    std::unique_lock<std::shared_mutex> lock(s.channelMutex);
    s.channelLevels.clear();
}

bool Log::IsEnabled(LogLevel level, std::string_view channel) noexcept {
    LogState& s = State();
    if (level == LogLevel::Off) return false;

    // 채널 오버라이드가 하나도 없으면 잠금 없이 끝낸다(핫패스).
    {
        std::shared_lock<std::shared_mutex> lock(s.channelMutex);
        if (!s.channelLevels.empty()) {
            auto it = s.channelLevels.find(channel);
            if (it != s.channelLevels.end()) return level >= it->second;
        }
    }
    return level >= s.globalLevel.load(std::memory_order_relaxed);
}

void Log::SetFrame(u64 frame) noexcept { State().frame.store(frame, std::memory_order_relaxed); }
u64  Log::Frame() noexcept { return State().frame.load(std::memory_order_relaxed); }

void Log::Submit(const LogRecord& record) {
    LogState& s = State();
    s.counts[static_cast<usize>(record.level)].fetch_add(1, std::memory_order_relaxed);

    std::shared_lock<std::shared_mutex> lock(s.sinkMutex);
    for (auto& sink : s.sinks) {
        if (record.level >= sink->minLevel) sink->Write(record);
    }
}

u64 Log::CountOf(LogLevel level) noexcept {
    return State().counts[static_cast<usize>(level)].load(std::memory_order_relaxed);
}

// ── LogEvent ───────────────────────────────────────────────────────────────
LogEvent::LogEvent(LogLevel level, std::string_view channel, std::string_view event, SourceLoc loc)
    : m_enabled(Log::IsEnabled(level, channel)) {
    if (!m_enabled) return;
    m_record.timestampNs = NowUnixNanos();
    m_record.level       = level;
    m_record.channel     = channel;
    m_record.event       = event;
    m_record.loc         = loc;
    m_record.frame       = Log::Frame();
    m_record.threadId    = CurrentThreadId();
}

LogEvent::~LogEvent() {
    if (m_enabled) Log::Submit(m_record);
}

LogEvent& LogEvent::Msg(std::string message) {
    if (m_enabled) m_record.message = std::move(message);
    return *this;
}

#define ALICE_LOG_FIELD_IMPL(ctype, member, tag)                                 \
    LogEvent& LogEvent::F(std::string_view key, ctype value) {                   \
        if (m_enabled) {                                                         \
            LogField f;                                                          \
            f.name   = std::string(key);                                         \
            f.type   = LogField::Type::tag;                                      \
            f.member = value;                                                    \
            m_record.fields.push_back(std::move(f));                             \
        }                                                                        \
        return *this;                                                            \
    }

ALICE_LOG_FIELD_IMPL(i64,  i, Int)
ALICE_LOG_FIELD_IMPL(f64,  f, Float)
ALICE_LOG_FIELD_IMPL(bool, b, Bool)
#undef ALICE_LOG_FIELD_IMPL

LogEvent& LogEvent::F(std::string_view key, std::string_view value) {
    if (m_enabled) {
        LogField f;
        f.name = std::string(key);
        f.type = LogField::Type::String;
        f.str  = std::string(value);
        m_record.fields.push_back(std::move(f));
    }
    return *this;
}

LogEvent& LogEvent::F(std::string_view key, const std::string& value) { return F(key, std::string_view(value)); }
LogEvent& LogEvent::F(std::string_view key, const char* value)        { return F(key, std::string_view(value ? value : "")); }
LogEvent& LogEvent::F(std::string_view key, i32 value)                { return F(key, static_cast<i64>(value)); }
LogEvent& LogEvent::F(std::string_view key, u64 value)                { return F(key, static_cast<i64>(value)); }
LogEvent& LogEvent::F(std::string_view key, u32 value)                { return F(key, static_cast<i64>(value)); }
LogEvent& LogEvent::F(std::string_view key, f32 value)                { return F(key, static_cast<f64>(value)); }

// ── 어서션 ─────────────────────────────────────────────────────────────────
void AssertFailed(const char* expr, SourceLoc loc, const char* message) noexcept {
    // 로거를 거치지 않는다. 로거 자신이 어서트할 수 있어야 하기 때문이다.
    std::fprintf(stderr, "\n[ASSERT] %s:%u  %s\n  expr: %s\n  msg : %s\n",
                 ShortFile(loc.file), loc.line, loc.func ? loc.func : "?",
                 expr, message ? message : "");
    std::fflush(stderr);
    Log::Flush();
    ALICE_DEBUGBREAK();
    std::abort();
}

} // namespace alice
