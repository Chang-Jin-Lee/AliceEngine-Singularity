// SPDX-License-Identifier: MIT
#include "TestFramework.h"

#include "Foundation/Diagnostic.h"
#include "Foundation/FileSystem.h"
#include "Foundation/Log.h"
#include "Foundation/Profiler.h"
#include "Foundation/Result.h"
#include "Foundation/StringUtil.h"

using namespace alice;

// ── StringUtil ─────────────────────────────────────────────────────────────
ALICE_TEST(StringUtil, TrimAndSplit) {
    ALICE_CHECK_STR(std::string(Trim("  hello  ")), "hello");
    ALICE_CHECK_STR(std::string(Trim("\t\nx\r\n")), "x");
    ALICE_CHECK_STR(std::string(Trim("   ")), "");

    const auto parts = Split("a,b,,c", ',');
    ALICE_REQUIRE(parts.size() == 4);
    ALICE_CHECK_STR(std::string(parts[0]), "a");
    ALICE_CHECK_STR(std::string(parts[2]), "");
    ALICE_CHECK_STR(std::string(parts[3]), "c");
}

ALICE_TEST(StringUtil, JsonEscape) {
    ALICE_CHECK_STR(JsonEscape("a\"b"), "a\\\"b");
    ALICE_CHECK_STR(JsonEscape("line\nbreak"), "line\\nbreak");
    // 한글은 UTF-8 바이트 그대로 통과해야 한다. \u 로 부풀리면 파일이 커지고 읽기 나빠진다.
    ALICE_CHECK_STR(JsonEscape("한글"), "한글");
    ALICE_CHECK_STR(JsonQuote("x"), "\"x\"");
}

ALICE_TEST(StringUtil, NumberRoundTrip) {
    // 실수는 정수처럼 보여도 ".0" 을 달아 타입을 지켜야 한다.
    ALICE_CHECK_STR(FormatDouble(3.0), "3.0");
    ALICE_CHECK_STR(FormatDouble(0.5), "0.5");

    i64 i = 0;
    ALICE_CHECK(ParseI64("42", i) && i == 42);
    ALICE_CHECK(ParseI64("-7", i) && i == -7);
    ALICE_CHECK(!ParseI64("42x", i));
    ALICE_CHECK(!ParseI64("4.2", i));

    f64 f = 0.0;
    ALICE_CHECK(ParseF64("1.5", f) && f == 1.5);
    ALICE_CHECK(!ParseF64("1.5.5", f));
}

ALICE_TEST(StringUtil, Format) {
    ALICE_CHECK_STR(Fmt("{} + {} = {}", 1, 2, 3), "1 + 2 = 3");
    ALICE_CHECK_STR(Fmt("no args"), "no args");
    // 인자가 남으면 조용히 버리지 않고 드러낸다.
    ALICE_CHECK(Fmt("{}", 1, 2).find("extra") != std::string::npos);
}

ALICE_TEST(StringUtil, EditDistance) {
    ALICE_CHECK(EditDistance("kitten", "sitting") == 3);
    ALICE_CHECK(EditDistance("abc", "abc") == 0);
    ALICE_CHECK(EditDistance("", "abc") == 3);
}

// ── Diagnostic ─────────────────────────────────────────────────────────────
ALICE_TEST(Diagnostic, ClosestMatchSuggestsTypo) {
    const std::vector<std::string> fields{"position", "rotation", "scale"};
    ALICE_CHECK_STR(ClosestMatch("positon", fields), "position");
    ALICE_CHECK_STR(ClosestMatch("rotaton", fields), "rotation");
    // 완전히 다른 이름에는 엉뚱한 제안을 하지 않아야 한다.
    ALICE_CHECK_STR(ClosestMatch("xyzzy", fields), "");
}

ALICE_TEST(Diagnostic, JsonShapeIsStable) {
    Diagnostic d;
    d.severity = Severity::Error;
    d.code     = "doc.schema.unknown_field";
    d.message  = "알 수 없는 필드";
    d.hint     = "position 을 뜻한 것인가?";
    d.file     = "player.actor.yaml";
    d.path     = "components.transform.positon";
    d.mark     = Mark{12, 5, 240};

    const std::string json = d.ToJson();
    ALICE_CHECK(json.find("\"severity\":\"error\"") != std::string::npos);
    ALICE_CHECK(json.find("\"code\":\"doc.schema.unknown_field\"") != std::string::npos);
    ALICE_CHECK(json.find("\"line\":12") != std::string::npos);
    ALICE_CHECK(json.find("\"column\":5") != std::string::npos);
    ALICE_CHECK(json.find("\"path\":\"components.transform.positon\"") != std::string::npos);
}

ALICE_TEST(Diagnostic, BagCountsAndSnippets) {
    DiagnosticBag bag;
    bag.Error("a.b", "첫 번째").mark = Mark{2, 1, 0};
    bag.Warning("c.d", "두 번째");

    ALICE_CHECK(bag.ErrorCount() == 1);
    ALICE_CHECK(bag.WarningCount() == 1);
    ALICE_CHECK(bag.HasErrors());

    bag.AttachSnippets("line one\nline two\nline three\n");
    ALICE_CHECK_STR(bag.Items()[0].snippet, "line two");
}

// ── Result ─────────────────────────────────────────────────────────────────
ALICE_TEST(Result, CarriesValueOrDiagnostic) {
    Result<int> ok{42};
    ALICE_CHECK(ok.IsOk());
    ALICE_CHECK(ok.Value() == 42);

    Result<int> err{MakeError("x.y", "터졌다")};
    ALICE_CHECK(err.IsErr());
    ALICE_CHECK_STR(err.Error().code, "x.y");
    ALICE_CHECK(err.ValueOr(7) == 7);
}

ALICE_TEST(Result, MovesNonCopyablePayload) {
    Result<std::string> r{std::string("hello")};
    ALICE_REQUIRE(r.IsOk());
    ALICE_CHECK_STR(*r, "hello");

    Result<std::string> moved = std::move(r);
    ALICE_CHECK_STR(*moved, "hello");
}

// ── Log ────────────────────────────────────────────────────────────────────
namespace {
/// 테스트가 관찰할 수 있게 레코드를 모아두는 싱크.
class CaptureSink : public ILogSink {
public:
    void Write(const LogRecord& record) override { records.push_back(record); }
    std::vector<LogRecord> records;
};
}

ALICE_TEST(Log, RecordCarriesStructuredFields) {
    auto sink = std::make_shared<CaptureSink>();
    Log::RemoveAllSinks();
    Log::AddSink(sink);
    Log::SetLevel(LogLevel::Trace);
    Log::SetFrame(7);

    ALICE_LOG_ERROR("asset", "asset.load.failed")
        .Msg("메시를 열지 못했다")
        .F("path", "meshes/player.mesh")
        .F("bytes", static_cast<i64>(1024))
        .F("cached", false);

    ALICE_REQUIRE(sink->records.size() == 1);
    const LogRecord& r = sink->records[0];
    ALICE_CHECK(r.level == LogLevel::Error);
    ALICE_CHECK_STR(std::string(r.channel), "asset");
    ALICE_CHECK_STR(std::string(r.event), "asset.load.failed");
    ALICE_CHECK(r.frame == 7);
    ALICE_REQUIRE(r.fields.size() == 3);

    const std::string json = r.ToJson();
    ALICE_CHECK(json.find("\"ev\":\"asset.load.failed\"") != std::string::npos);
    ALICE_CHECK(json.find("\"frame\":7") != std::string::npos);
    ALICE_CHECK(json.find("\"bytes\":1024") != std::string::npos);
    ALICE_CHECK(json.find("\"cached\":false") != std::string::npos);

    Log::RemoveAllSinks();
    Log::SetLevel(LogLevel::Error);
}

ALICE_TEST(Log, LevelFilteringSkipsArgumentEvaluation) {
    auto sink = std::make_shared<CaptureSink>();
    Log::RemoveAllSinks();
    Log::AddSink(sink);
    Log::SetLevel(LogLevel::Warn);

    int evaluated = 0;
    auto expensive = [&]() { ++evaluated; return std::string("expensive"); };

    ALICE_LOG_DEBUG("test", "test.skipped").F("v", expensive());
    ALICE_CHECK(evaluated == 0);
    ALICE_CHECK(sink->records.empty());

    ALICE_LOG_WARN("test", "test.kept").F("v", expensive());
    ALICE_CHECK(evaluated == 1);
    ALICE_CHECK(sink->records.size() == 1);

    Log::RemoveAllSinks();
    Log::SetLevel(LogLevel::Error);
}

ALICE_TEST(Log, ChannelLevelOverridesGlobal) {
    auto sink = std::make_shared<CaptureSink>();
    Log::RemoveAllSinks();
    Log::AddSink(sink);
    Log::SetLevel(LogLevel::Error);
    Log::SetChannelLevel("rhi", LogLevel::Trace);

    ALICE_LOG_TRACE("rhi", "rhi.detail").Msg("보여야 한다");
    ALICE_LOG_TRACE("gameplay", "gameplay.detail").Msg("가려져야 한다");

    ALICE_CHECK(sink->records.size() == 1);
    if (!sink->records.empty()) {
        ALICE_CHECK_STR(std::string(sink->records[0].channel), "rhi");
    }

    Log::ClearChannelLevels();
    Log::RemoveAllSinks();
}

ALICE_TEST(Log, RingSinkQueriesRecentRecords) {
    RingLogSink ring(4);
    for (int i = 0; i < 6; ++i) {
        LogRecord r;
        r.level   = (i % 2 == 0) ? LogLevel::Info : LogLevel::Error;
        r.channel = "test";
        r.event   = "test.tick";
        ring.Write(r);
    }
    ALICE_CHECK(ring.Size() == 4);   // 링버퍼가 오래된 것을 밀어냈다
    ALICE_CHECK(ring.Tail(2).size() == 2);
    ALICE_CHECK(ring.Query(LogLevel::Error, {}, {}, 10).size() == 2);
    ALICE_CHECK(ring.Query(LogLevel::Trace, "nope", {}, 10).empty());
}

// ── Profiler ───────────────────────────────────────────────────────────────
ALICE_TEST(Profiler, ZonesNestAndAggregate) {
    Profiler::Reset();
    Profiler::SetEnabled(true);
    Profiler::BeginFrame(1);
    {
        ALICE_PROFILE_ZONE("Outer");
        {
            ALICE_PROFILE_ZONE("Inner");
            volatile int sink = 0;
            for (int i = 0; i < 20000; ++i) sink += i;
            ALICE_UNUSED(sink);
        }
    }
    Profiler::EndFrame();

    FrameData frame;
    ALICE_REQUIRE(Profiler::LatestFrame(frame));
    ALICE_CHECK(frame.frameIndex == 1);
    ALICE_CHECK(frame.zones.size() == 2);

    const auto agg = Profiler::Aggregate(1);
    ALICE_REQUIRE(agg.size() == 2);
    // Inner 가 실제 일을 했으므로 self 시간이 더 커야 한다 → 정렬 첫 자리.
    ALICE_CHECK_STR(agg[0].name, "Inner");

    // 부모의 total 은 자식을 포함한다.
    for (const auto& a : agg) {
        if (a.name == "Outer") ALICE_CHECK(a.totalNs >= a.selfNs);
    }
    Profiler::Reset();
}

ALICE_TEST(Profiler, BudgetViolationIsLogged) {
    auto sink = std::make_shared<CaptureSink>();
    Log::RemoveAllSinks();
    Log::AddSink(sink);
    Log::SetLevel(LogLevel::Trace);

    Profiler::Reset();
    Profiler::SetEnabled(true);
    Profiler::ClearBudgets();
    Profiler::SetBudgetMs("Slow", 0.0);   // 무조건 초과하는 예산

    Profiler::BeginFrame(1);
    {
        ALICE_PROFILE_ZONE("Slow");
        // 존이 측정 가능한 시간을 쓰게 만든다. 빈 스코프는 시계 분해능 안에서 0ms 로
        // 나올 수 있고, 그러면 예산 0.0 을 "초과하지 않은" 것이 되어 테스트가 흔들린다.
        volatile u64 workSum = 0;
        for (int i = 0; i < 200000; ++i) workSum += static_cast<u64>(i);
        ALICE_UNUSED(workSum);
    }
    Profiler::EndFrame();

    bool found = false;
    for (const LogRecord& r : sink->records) {
        if (r.event == "perf.budget.exceeded") found = true;
    }
    ALICE_CHECK_MSG(found, "예산 초과가 구조화 로그로 나와야 한다");
    ALICE_CHECK(Profiler::BudgetViolationCount() >= 1);

    Profiler::ClearBudgets();
    Profiler::Reset();
    Log::RemoveAllSinks();
    Log::SetLevel(LogLevel::Error);
}

ALICE_TEST(Profiler, HotspotsJsonIsMachineReadable) {
    Profiler::Reset();
    Profiler::SetEnabled(true);
    Profiler::BeginFrame(42);
    { ALICE_PROFILE_ZONE("Render"); }
    Profiler::EndFrame();

    const std::string json = Profiler::HotspotsJson(5, 1);
    ALICE_CHECK(json.find("\"frame\":42") != std::string::npos);
    ALICE_CHECK(json.find("\"hotspots\":[") != std::string::npos);
    ALICE_CHECK(json.find("\"name\":\"Render\"") != std::string::npos);
    ALICE_CHECK(json.find("\"p99Ms\"") != std::string::npos);
    Profiler::Reset();
}

// ── FileSystem ─────────────────────────────────────────────────────────────
ALICE_TEST(FileSystem, PathHelpers) {
    ALICE_CHECK_STR(fs::Extension("a/b/player.actor.yaml"), ".yaml");
    ALICE_CHECK_STR(fs::FileName("a/b/player.actor.yaml"), "player.actor.yaml");
    ALICE_CHECK_STR(fs::FileStem("a/b/player.actor.yaml"), "player.actor");
    ALICE_CHECK_STR(fs::ParentPath("a/b/player.yaml"), "a/b");
    ALICE_CHECK_STR(fs::Normalize("a\\b\\\\c"), "a/b/c");
    ALICE_CHECK_STR(fs::Join("a/b", "c.yaml"), "a/b/c.yaml");
    ALICE_CHECK_STR(fs::Join("a/b/", "/c.yaml"), "a/b/c.yaml");
}

ALICE_TEST(FileSystem, TextRoundTripStripsBomAndCrlf) {
    const std::string dir  = "AliceTestTmp";
    const std::string path = dir + "/roundtrip.txt";

    ALICE_REQUIRE(fs::CreateDirectories(dir).IsOk());
    ALICE_REQUIRE(fs::WriteTextFile(path, "\xEF\xBB\xBF" "alpha\r\nbeta\r\n").IsOk());

    Result<std::string> read = fs::ReadTextFile(path);
    ALICE_REQUIRE(read.IsOk());
    ALICE_CHECK_STR(*read, "alpha\nbeta\n");

    ALICE_CHECK(fs::ReadTextFile(dir + "/does-not-exist").IsErr());
}
