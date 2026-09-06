// SPDX-License-Identifier: MIT
#include "Diagnostic.h"
#include "StringUtil.h"

#include <algorithm>

namespace alice {

const char* ToString(Severity s) noexcept {
    switch (s) {
        case Severity::Note:    return "note";
        case Severity::Warning: return "warning";
        case Severity::Error:   return "error";
    }
    return "error";
}

std::string Diagnostic::ToLine() const {
    std::string out;
    out.reserve(128);

    out += file.empty() ? "<memory>" : file;
    if (mark.Valid()) {
        out += ':';
        detail::FormatAppend(out, mark.line);
        out += ':';
        detail::FormatAppend(out, mark.column);
    }
    out += ": ";
    out += ToString(severity);
    if (!code.empty()) { out += '['; out += code; out += ']'; }
    out += ": ";
    out += message;
    if (!path.empty()) { out += "  (at "; out += path; out += ')'; }
    return out;
}

std::string Diagnostic::ToPretty() const {
    std::string out = ToLine();
    if (!snippet.empty()) {
        out += "\n  | ";
        out += snippet;
        if (mark.column > 0) {
            out += "\n  | ";
            // 캐럿 위치는 바이트가 아니라 열 번호 기준이다. 탭은 공백 한 칸으로 센다.
            out.append(static_cast<usize>(mark.column - 1), ' ');
            out += '^';
        }
    }
    if (!hint.empty()) {
        out += "\n  = hint: ";
        out += hint;
    }
    return out;
}

std::string Diagnostic::ToJson() const {
    std::string out;
    out.reserve(256);
    out += "{\"severity\":";
    out += JsonQuote(ToString(severity));
    out += ",\"code\":";
    out += JsonQuote(code);
    out += ",\"message\":";
    out += JsonQuote(message);
    if (!hint.empty()) { out += ",\"hint\":";    out += JsonQuote(hint); }
    if (!file.empty()) { out += ",\"file\":";    out += JsonQuote(file); }
    if (!path.empty()) { out += ",\"path\":";    out += JsonQuote(path); }
    if (mark.Valid()) {
        out += ",\"line\":";   detail::FormatAppend(out, mark.line);
        out += ",\"column\":"; detail::FormatAppend(out, mark.column);
        out += ",\"offset\":"; detail::FormatAppend(out, mark.offset);
    }
    if (!snippet.empty()) { out += ",\"snippet\":"; out += JsonQuote(snippet); }
    out += '}';
    return out;
}

void DiagnosticBag::Add(Diagnostic d) {
    if (d.severity == Severity::Error)   ++m_errorCount;
    if (d.severity == Severity::Warning) ++m_warningCount;
    m_items.push_back(std::move(d));
}

Diagnostic& DiagnosticBag::Error(std::string code, std::string message) {
    Diagnostic d;
    d.severity = Severity::Error;
    d.code     = std::move(code);
    d.message  = std::move(message);
    Add(std::move(d));
    return m_items.back();
}

Diagnostic& DiagnosticBag::Warning(std::string code, std::string message) {
    Diagnostic d;
    d.severity = Severity::Warning;
    d.code     = std::move(code);
    d.message  = std::move(message);
    Add(std::move(d));
    return m_items.back();
}

Diagnostic& DiagnosticBag::Note(std::string code, std::string message) {
    Diagnostic d;
    d.severity = Severity::Note;
    d.code     = std::move(code);
    d.message  = std::move(message);
    Add(std::move(d));
    return m_items.back();
}

void DiagnosticBag::Clear() noexcept {
    m_items.clear();
    m_errorCount   = 0;
    m_warningCount = 0;
}

void DiagnosticBag::Append(const DiagnosticBag& other) {
    for (const Diagnostic& d : other.m_items) Add(d);
}

void DiagnosticBag::SetFileIfEmpty(const std::string& file) {
    for (Diagnostic& d : m_items) {
        if (d.file.empty()) d.file = file;
    }
}

void DiagnosticBag::AttachSnippets(const std::string& sourceText) {
    if (sourceText.empty()) return;

    // 줄 시작 오프셋 표를 한 번만 만든다.
    std::vector<usize> lineStarts;
    lineStarts.push_back(0);
    for (usize i = 0; i < sourceText.size(); ++i) {
        if (sourceText[i] == '\n') lineStarts.push_back(i + 1);
    }

    for (Diagnostic& d : m_items) {
        if (!d.mark.Valid() || !d.snippet.empty()) continue;
        const usize idx = d.mark.line - 1;
        if (idx >= lineStarts.size()) continue;
        const usize begin = lineStarts[idx];
        usize end = (idx + 1 < lineStarts.size()) ? lineStarts[idx + 1] : sourceText.size();
        while (end > begin && (sourceText[end - 1] == '\n' || sourceText[end - 1] == '\r')) --end;
        d.snippet = sourceText.substr(begin, end - begin);
    }
}

std::string DiagnosticBag::ToPretty() const {
    std::string out;
    for (const Diagnostic& d : m_items) {
        out += d.ToPretty();
        out += '\n';
    }
    return out;
}

std::string DiagnosticBag::ToJsonLines() const {
    std::string out;
    for (const Diagnostic& d : m_items) {
        out += d.ToJson();
        out += '\n';
    }
    return out;
}

std::string DiagnosticBag::ToJsonObject() const {
    std::string out = "{\"errors\":";
    detail::FormatAppend(out, static_cast<u64>(m_errorCount));
    out += ",\"warnings\":";
    detail::FormatAppend(out, static_cast<u64>(m_warningCount));
    out += ",\"diagnostics\":[";
    for (usize i = 0; i < m_items.size(); ++i) {
        if (i) out += ',';
        out += m_items[i].ToJson();
    }
    out += "]}";
    return out;
}

std::string ClosestMatch(std::string_view name, const std::vector<std::string>& candidates) {
    if (name.empty() || candidates.empty()) return {};

    // 임계값은 길이에 비례해 느슨해진다. 짧은 이름에 엉뚱한 제안을 하지 않기 위해서다.
    const usize limit = std::max<usize>(1, name.size() / 3 + 1);

    const std::string* best = nullptr;
    usize bestDistance = limit + 1;
    for (const std::string& c : candidates) {
        const usize d = EditDistance(name, c);
        if (d < bestDistance) { bestDistance = d; best = &c; }
    }
    return (best && bestDistance <= limit) ? *best : std::string{};
}

} // namespace alice
