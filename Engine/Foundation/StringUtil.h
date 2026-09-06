// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Foundation/StringUtil.h
#pragma once

#include "Core.h"

#include <string>
#include <string_view>
#include <vector>

namespace alice {

// ── 기본 조작 ──────────────────────────────────────────────────────────────
bool StartsWith(std::string_view s, std::string_view prefix) noexcept;
bool EndsWith(std::string_view s, std::string_view suffix) noexcept;
bool EqualsIgnoreCase(std::string_view a, std::string_view b) noexcept;

std::string_view TrimLeft(std::string_view s) noexcept;
std::string_view TrimRight(std::string_view s) noexcept;
std::string_view Trim(std::string_view s) noexcept;

std::string ToLower(std::string_view s);
std::string ToUpper(std::string_view s);

std::vector<std::string_view> Split(std::string_view s, char delim);
std::string Join(const std::vector<std::string>& parts, std::string_view sep);
std::string Replace(std::string_view s, std::string_view from, std::string_view to);

// ── 직렬화 보조 ────────────────────────────────────────────────────────────
/// JSON 문자열 리터럴 본문으로 이스케이프한다(따옴표는 포함하지 않는다).
/// 제어문자는 \uXXXX, 비ASCII UTF-8 바이트는 그대로 통과시킨다(JSON은 UTF-8을 허용).
std::string JsonEscape(std::string_view s);

/// 따옴표까지 붙인 완전한 JSON 문자열.
std::string JsonQuote(std::string_view s);

/// double 을 왕복 가능(round-trip)하게 찍는다. 정수값이면 ".0" 을 붙여 타입을 보존한다.
std::string FormatDouble(f64 v);

/// 숫자 파싱. 전체 문자열이 소비되어야 성공.
bool ParseI64(std::string_view s, i64& out) noexcept;
bool ParseF64(std::string_view s, f64& out) noexcept;

// ── 편집 거리 ──────────────────────────────────────────────────────────────
/// Levenshtein 거리. 오타 힌트("did you mean ...")에 쓴다.
usize EditDistance(std::string_view a, std::string_view b);

// ── 아주 작은 포매터 ───────────────────────────────────────────────────────
// std::format 은 Apple Clang / 구형 NDK 에서 아직 편차가 커서 직접 둔다.
// "{}" 를 인자로 순서대로 치환한다. 그 이상은 하지 않는다 — 로그 메시지엔 그거면 충분하다.
namespace detail {
    void FormatAppend(std::string& out, std::string_view v);
    void FormatAppend(std::string& out, const std::string& v);
    void FormatAppend(std::string& out, const char* v);
    void FormatAppend(std::string& out, char v);
    void FormatAppend(std::string& out, bool v);
    void FormatAppend(std::string& out, i32 v);
    void FormatAppend(std::string& out, i64 v);
    void FormatAppend(std::string& out, u32 v);
    void FormatAppend(std::string& out, u64 v);
    void FormatAppend(std::string& out, f32 v);
    void FormatAppend(std::string& out, f64 v);
    void FormatAppend(std::string& out, const void* v);
#if defined(_MSC_VER)
    inline void FormatAppend(std::string& out, unsigned long v) { FormatAppend(out, static_cast<u64>(v)); }
    inline void FormatAppend(std::string& out, long v)          { FormatAppend(out, static_cast<i64>(v)); }
#endif

    /// 자리표시자 하나를 찾아 out 에 앞부분을 밀어넣고, 남은 포맷 문자열을 돌려준다.
    std::string_view FormatNext(std::string& out, std::string_view fmt, bool& found);
}

inline void FormatTo(std::string& out, std::string_view fmt) {
    // 남은 자리표시자는 그대로 둔다 — 인자 개수 불일치를 숨기지 않기 위해서다.
    out.append(fmt);
}

template <typename Arg, typename... Rest>
void FormatTo(std::string& out, std::string_view fmt, const Arg& arg, const Rest&... rest) {
    bool found = false;
    std::string_view tail = detail::FormatNext(out, fmt, found);
    if (!found) {
        // 인자가 남았는데 자리표시자가 없다. 조용히 버리지 않고 뒤에 붙여 드러낸다.
        out.append(" <extra:");
        detail::FormatAppend(out, arg);
        out.append(">");
        FormatTo(out, std::string_view{}, rest...);
        return;
    }
    detail::FormatAppend(out, arg);
    FormatTo(out, tail, rest...);
}

template <typename... Args>
std::string Fmt(std::string_view fmt, const Args&... args) {
    std::string out;
    out.reserve(fmt.size() + 32);
    FormatTo(out, fmt, args...);
    return out;
}

} // namespace alice
