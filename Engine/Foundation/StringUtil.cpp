// SPDX-License-Identifier: MIT
#include "StringUtil.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace alice {
namespace {
    constexpr std::string_view kSpaces = " \t\r\n\f\v";

    inline char LowerAscii(char c) noexcept {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }
    inline char UpperAscii(char c) noexcept {
        return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    }
}

bool StartsWith(std::string_view s, std::string_view prefix) noexcept {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool EndsWith(std::string_view s, std::string_view suffix) noexcept {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (usize i = 0; i < a.size(); ++i) {
        if (LowerAscii(a[i]) != LowerAscii(b[i])) return false;
    }
    return true;
}

std::string_view TrimLeft(std::string_view s) noexcept {
    const usize p = s.find_first_not_of(kSpaces);
    return p == std::string_view::npos ? std::string_view{} : s.substr(p);
}

std::string_view TrimRight(std::string_view s) noexcept {
    const usize p = s.find_last_not_of(kSpaces);
    return p == std::string_view::npos ? std::string_view{} : s.substr(0, p + 1);
}

std::string_view Trim(std::string_view s) noexcept { return TrimRight(TrimLeft(s)); }

std::string ToLower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = LowerAscii(c);
    return out;
}

std::string ToUpper(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = UpperAscii(c);
    return out;
}

std::vector<std::string_view> Split(std::string_view s, char delim) {
    std::vector<std::string_view> parts;
    if (s.empty()) return parts;
    usize start = 0;
    while (true) {
        const usize p = s.find(delim, start);
        if (p == std::string_view::npos) { parts.push_back(s.substr(start)); break; }
        parts.push_back(s.substr(start, p - start));
        start = p + 1;
    }
    return parts;
}

std::string Join(const std::vector<std::string>& parts, std::string_view sep) {
    std::string out;
    for (usize i = 0; i < parts.size(); ++i) {
        if (i) out.append(sep);
        out.append(parts[i]);
    }
    return out;
}

std::string Replace(std::string_view s, std::string_view from, std::string_view to) {
    if (from.empty()) return std::string(s);
    std::string out;
    usize start = 0;
    while (true) {
        const usize p = s.find(from, start);
        if (p == std::string_view::npos) { out.append(s.substr(start)); break; }
        out.append(s.substr(start, p - start));
        out.append(to);
        start = p + from.size();
    }
    return out;
}

std::string JsonEscape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (const char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out += ch;   // UTF-8 바이트는 그대로. JSON 은 UTF-8 을 허용한다.
                }
                break;
        }
    }
    return out;
}

std::string JsonQuote(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    out += JsonEscape(s);
    out += '"';
    return out;
}

std::string FormatDouble(f64 v) {
    if (std::isnan(v)) return "null";          // JSON 에 NaN 리터럴이 없다.
    if (std::isinf(v)) return v > 0 ? "1e999" : "-1e999";

    char buf[64];
    std::string out;
#if defined(_MSC_VER)
    const auto res = std::to_chars(buf, buf + sizeof(buf), v);
    out.assign(buf, res.ptr);
#else
    const int n = std::snprintf(buf, sizeof(buf), "%.17g", v);
    out.assign(buf, buf + (n > 0 ? n : 0));
#endif
    // 정수처럼 보이면 ".0" 을 붙인다. 그래야 JSON 왕복에서 실수 타입이 정수로 무너지지 않는다.
    if (out.find_first_of(".eEnN") == std::string::npos) out += ".0";
    return out;
}

bool ParseI64(std::string_view s, i64& out) noexcept {
    if (s.empty()) return false;
    const char* first = s.data();
    const char* last  = s.data() + s.size();
    int base = 10;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { first += 2; base = 16; }
    const auto res = std::from_chars(first, last, out, base);
    return res.ec == std::errc{} && res.ptr == last;
}

bool ParseF64(std::string_view s, f64& out) noexcept {
    if (s.empty()) return false;
#if defined(_MSC_VER)
    const auto res = std::from_chars(s.data(), s.data() + s.size(), out);
    return res.ec == std::errc{} && res.ptr == s.data() + s.size();
#else
    // libstdc++/libc++ 의 부동소수 from_chars 지원 편차를 피해 strtod 로 간다.
    std::string tmp(s);
    char* end = nullptr;
    const double v = std::strtod(tmp.c_str(), &end);
    if (end != tmp.c_str() + tmp.size()) return false;
    out = v;
    return true;
#endif
}

usize EditDistance(std::string_view a, std::string_view b) {
    if (a.empty()) return b.size();
    if (b.empty()) return a.size();

    std::vector<usize> prev(b.size() + 1);
    std::vector<usize> cur(b.size() + 1);
    for (usize j = 0; j <= b.size(); ++j) prev[j] = j;

    for (usize i = 1; i <= a.size(); ++i) {
        cur[0] = i;
        for (usize j = 1; j <= b.size(); ++j) {
            const usize cost = (LowerAscii(a[i - 1]) == LowerAscii(b[j - 1])) ? 0u : 1u;
            cur[j] = std::min({ prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost });
        }
        prev.swap(cur);
    }
    return prev[b.size()];
}

namespace detail {

void FormatAppend(std::string& out, std::string_view v)   { out.append(v); }
void FormatAppend(std::string& out, const std::string& v) { out.append(v); }
void FormatAppend(std::string& out, const char* v)        { out.append(v ? v : "(null)"); }
void FormatAppend(std::string& out, char v)               { out.push_back(v); }
void FormatAppend(std::string& out, bool v)               { out.append(v ? "true" : "false"); }

void FormatAppend(std::string& out, i64 v) {
    char buf[24];
    const auto res = std::to_chars(buf, buf + sizeof(buf), v);
    out.append(buf, res.ptr);
}
void FormatAppend(std::string& out, u64 v) {
    char buf[24];
    const auto res = std::to_chars(buf, buf + sizeof(buf), v);
    out.append(buf, res.ptr);
}
void FormatAppend(std::string& out, i32 v) { FormatAppend(out, static_cast<i64>(v)); }
void FormatAppend(std::string& out, u32 v) { FormatAppend(out, static_cast<u64>(v)); }
void FormatAppend(std::string& out, f32 v) { out.append(FormatDouble(static_cast<f64>(v))); }
void FormatAppend(std::string& out, f64 v) { out.append(FormatDouble(v)); }

void FormatAppend(std::string& out, const void* v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%llx",
                  static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(v)));
    out.append(buf);
}

std::string_view FormatNext(std::string& out, std::string_view fmt, bool& found) {
    for (usize i = 0; i + 1 < fmt.size(); ++i) {
        if (fmt[i] == '{' && fmt[i + 1] == '}') {
            out.append(fmt.substr(0, i));
            found = true;
            return fmt.substr(i + 2);
        }
    }
    out.append(fmt);
    found = false;
    return std::string_view{};
}

} // namespace detail
} // namespace alice
