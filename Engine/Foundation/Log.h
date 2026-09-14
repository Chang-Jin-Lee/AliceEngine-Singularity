// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Foundation/Log.h
//
// 이 엔진의 로그는 **문자열이 아니라 레코드**다.
//
// 왜: 요구사항은 "AI가 수많은 로그를 파악해 원인을 좁힌다" 이다. 사람이 읽는 문장만
// 뱉으면 AI는 매번 정규식을 새로 짜야 하고, 문장이 조금만 바뀌어도 깨진다. 그래서
// 모든 로그는 (1) 절대 바뀌지 않는 이벤트 ID, (2) 타입이 있는 필드 집합, (3) 사람용
// 문장 — 셋을 함께 들고 다닌다. 같은 레코드가 콘솔에는 문장으로, 파일에는 NDJSON
// 한 줄로 나간다. AI는 NDJSON 만 읽으면 되고 사람은 콘솔만 보면 된다.
//
//   ALICE_LOG_ERROR("asset", "asset.load.failed", "메시를 열지 못했다")
//       .F("path", path).F("reason", "not found").F("bytes", size);
//
//   → 콘솔: [E] asset  asset.load.failed  메시를 열지 못했다  path=... reason=...
//   → JSONL: {"ts":...,"lv":"error","ch":"asset","ev":"asset.load.failed", ... }
//
// 이벤트 ID 규칙: "<채널>.<대상>.<결과>" 소문자 점 표기. 한 번 릴리스되면 바꾸지 않는다.
// 바꿔야 하면 새 ID를 만들고 옛 ID를 Docs/LOG_EVENTS.md 에서 deprecated 로 표시한다.
#pragma once

#include "Core.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace alice {

enum class LogLevel : u8 {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
    Off,
};

const char* ToString(LogLevel level) noexcept;
bool        ParseLogLevel(std::string_view s, LogLevel& out) noexcept;

/// 로그 필드 하나. 타입을 유지해야 AI 쪽에서 숫자 비교/집계가 된다.
struct LogField {
    enum class Type : u8 { String, Int, Float, Bool };

    std::string name;
    Type        type = Type::String;
    std::string str;
    i64         i = 0;
    f64         f = 0.0;
    bool        b = false;

    std::string ValueToJson() const;
    std::string ValueToText() const;
};

struct LogRecord {
    i64                   timestampNs = 0;   ///< UNIX epoch 기준 나노초
    LogLevel              level       = LogLevel::Info;
    std::string_view      channel;           ///< 정적 문자열이어야 한다
    std::string_view      event;             ///< 정적 문자열이어야 한다
    std::string           message;
    std::vector<LogField> fields;
    SourceLoc             loc;
    u64                   frame    = 0;
    u32                   threadId = 0;

    std::string ToText() const;   ///< 사람용 한 줄
    std::string ToJson() const;   ///< 기계용 한 줄 (NDJSON)
};

/// 로그 목적지. 스레드 안전해야 한다 — Log 는 싱크 호출을 직렬화하지 않는다.
class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void Write(const LogRecord& record) = 0;
    virtual void Flush() {}
    /// 싱크별 최소 레벨. Off 면 아무것도 받지 않는다.
    LogLevel minLevel = LogLevel::Trace;
};

// ── 표준 싱크 ──────────────────────────────────────────────────────────────

/// 사람이 보는 콘솔. 터미널이면 색을 넣는다.
std::shared_ptr<ILogSink> MakeConsoleSink(bool color = true);

/// 사람이 보는 텍스트 파일.
std::shared_ptr<ILogSink> MakeTextFileSink(const std::string& path);

/// **AI가 읽는 파일.** 줄마다 JSON 하나(NDJSON). 이게 기본 계약이다.
std::shared_ptr<ILogSink> MakeJsonlFileSink(const std::string& path);

/// 메모리 링버퍼. 에디터 로그 패널과 엔진 내장 AI가 최근 로그를 질의할 때 쓴다.
class RingLogSink : public ILogSink {
public:
    explicit RingLogSink(usize capacity = 8192);
    void Write(const LogRecord& record) override;

    /// 최근 N개를 시간순으로 복사해 준다.
    std::vector<LogRecord> Tail(usize count) const;
    /// 필터 질의. 빈 문자열은 "무시"를 뜻한다.
    std::vector<LogRecord> Query(LogLevel minimumLevel,
                                 std::string_view channel,
                                 std::string_view eventPrefix,
                                 usize limit) const;
    void  Clear();
    usize Size() const;

private:
    struct Impl;
    std::shared_ptr<Impl> m_impl;
};

// ── 파사드 ─────────────────────────────────────────────────────────────────
class Log {
public:
    static void Initialize();
    static void Shutdown();

    static void AddSink(std::shared_ptr<ILogSink> sink);
    static void RemoveAllSinks();
    static void Flush();

    /// 전역 최소 레벨.
    static void     SetLevel(LogLevel level) noexcept;
    static LogLevel GetLevel() noexcept;

    /// 채널별 최소 레벨. 전역보다 우선한다.
    /// 예) 렌더 병목을 쫓을 때 "rhi" 만 Trace 로 내린다.
    static void SetChannelLevel(std::string_view channel, LogLevel level);
    static void ClearChannelLevels();

    /// 활성 여부. 매크로가 인자 평가 전에 먼저 물어본다.
    static bool IsEnabled(LogLevel level, std::string_view channel) noexcept;

    /// 프레임 번호. 모든 레코드에 실린다 — AI가 "몇 프레임에 무슨 일이 있었나"를
    /// 시간이 아니라 프레임으로 볼 수 있어야 렌더링 문제를 짚을 수 있다.
    static void SetFrame(u64 frame) noexcept;
    static u64  Frame() noexcept;

    static void Submit(const LogRecord& record);

    /// 지금까지 나온 레벨별 개수. 테스트와 CI 게이트에 쓴다.
    static u64 CountOf(LogLevel level) noexcept;
};

/// 소멸자에서 레코드를 발행하는 빌더. 임시 객체로 쓰인다.
class LogEvent {
public:
    LogEvent(LogLevel level, std::string_view channel, std::string_view event, SourceLoc loc);
    ~LogEvent();

    LogEvent(const LogEvent&) = delete;
    LogEvent& operator=(const LogEvent&) = delete;

    LogEvent& Msg(std::string message);

    template <typename... Args>
    LogEvent& Msg(std::string_view fmt, const Args&... args);

    LogEvent& F(std::string_view key, std::string_view value);
    LogEvent& F(std::string_view key, const std::string& value);
    LogEvent& F(std::string_view key, const char* value);
    LogEvent& F(std::string_view key, i64 value);
    LogEvent& F(std::string_view key, i32 value);
    LogEvent& F(std::string_view key, u64 value);
    LogEvent& F(std::string_view key, u32 value);
    LogEvent& F(std::string_view key, f64 value);
    LogEvent& F(std::string_view key, f32 value);
    LogEvent& F(std::string_view key, bool value);

private:
    LogRecord m_record;
    bool      m_enabled;
};

/// 비활성 레벨에서 인자 평가를 건너뛰기 위한 빈 껍데기.
class LogEventNull {
public:
    LogEventNull& Msg(std::string) { return *this; }
    template <typename... Args>
    LogEventNull& Msg(std::string_view, const Args&...) { return *this; }
    template <typename T>
    LogEventNull& F(std::string_view, const T&) { return *this; }
};

} // namespace alice

// ── 매크로 ─────────────────────────────────────────────────────────────────
// 삼항으로 감싸 비활성 시 인자 평가 자체를 없앤다.
// (LogEvent 와 LogEventNull 이 서로 다른 타입이므로 값으로 반환할 수 없다 → 즉시 소비한다.)
#define ALICE_LOG_AT(level, channel, event)                                            \
    if (!::alice::Log::IsEnabled(level, channel)) {} else                              \
        ::alice::LogEvent(level, channel, event, ALICE_HERE)

#define ALICE_LOG_TRACE(channel, event) ALICE_LOG_AT(::alice::LogLevel::Trace, channel, event)
#define ALICE_LOG_DEBUG(channel, event) ALICE_LOG_AT(::alice::LogLevel::Debug, channel, event)
#define ALICE_LOG_INFO(channel, event)  ALICE_LOG_AT(::alice::LogLevel::Info,  channel, event)
#define ALICE_LOG_WARN(channel, event)  ALICE_LOG_AT(::alice::LogLevel::Warn,  channel, event)
#define ALICE_LOG_ERROR(channel, event) ALICE_LOG_AT(::alice::LogLevel::Error, channel, event)
#define ALICE_LOG_FATAL(channel, event) ALICE_LOG_AT(::alice::LogLevel::Fatal, channel, event)

#include "StringUtil.h"

namespace alice {
template <typename... Args>
LogEvent& LogEvent::Msg(std::string_view fmt, const Args&... args) {
    return Msg(Fmt(fmt, args...));
}
}
