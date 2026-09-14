// SPDX-License-Identifier: MIT
#include "TestFramework.h"

#include "Foundation/Log.h"
#include "Foundation/Time.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace alice::test {

void Context::Fail(std::string expression, std::string detail, const char* file, u32 line) {
    Failure f;
    f.expression = std::move(expression);
    f.detail     = std::move(detail);
    f.file       = ShortFile(file);
    f.line       = line;
    m_failures.push_back(std::move(f));
}

Registry& Registry::Instance() {
    static Registry r;
    return r;
}

void Registry::Add(std::string suite, std::string name, CaseFn fn) {
    m_entries.push_back(Entry{std::move(suite), std::move(name), std::move(fn)});
}

namespace {

std::string FullName(const Registry::Entry& e) { return e.suite + "." + e.name; }

std::string ResultsToJson(const std::vector<CaseResult>& results, u32 passed, u32 failed, f64 totalMs) {
    std::string out = "{\"passed\":";
    detail::FormatAppend(out, passed);
    out += ",\"failed\":";
    detail::FormatAppend(out, failed);
    out += ",\"totalMs\":";
    out += FormatDouble(totalMs);
    out += ",\"cases\":[";
    for (usize i = 0; i < results.size(); ++i) {
        const CaseResult& r = results[i];
        if (i) out += ',';
        out += "{\"suite\":";  out += JsonQuote(r.suite);
        out += ",\"name\":";   out += JsonQuote(r.name);
        out += ",\"passed\":"; out += (r.passed ? "true" : "false");
        out += ",\"ms\":";     out += FormatDouble(r.ms);
        if (!r.failures.empty()) {
            out += ",\"failures\":[";
            for (usize j = 0; j < r.failures.size(); ++j) {
                const Failure& f = r.failures[j];
                if (j) out += ',';
                out += "{\"expr\":";  out += JsonQuote(f.expression);
                out += ",\"detail\":"; out += JsonQuote(f.detail);
                out += ",\"file\":";   out += JsonQuote(f.file);
                out += ",\"line\":";   detail::FormatAppend(out, f.line);
                out += '}';
            }
            out += ']';
        }
        out += '}';
    }
    out += "]}";
    return out;
}

} // namespace

int RunAll(const std::string& filter, bool json) {
    const std::vector<Registry::Entry>& entries = Registry::Instance().Entries();

    std::vector<CaseResult> results;
    u32 passed = 0;
    u32 failed = 0;
    Stopwatch total;

    for (const Registry::Entry& e : entries) {
        const std::string full = FullName(e);
        if (!filter.empty() && full.find(filter) == std::string::npos) continue;

        CaseResult r;
        r.suite = e.suite;
        r.name  = e.name;

        Context ctx;
        Stopwatch sw;
        e.fn(ctx);
        r.ms = sw.ElapsedMillis();

        r.failures = ctx.Failures();
        r.passed   = r.failures.empty();
        (r.passed ? passed : failed) += 1;

        if (!json) {
            if (r.passed) {
                std::printf("  ok    %-46s %6.2f ms\n", full.c_str(), r.ms);
            } else {
                std::printf("  FAIL  %-46s %6.2f ms\n", full.c_str(), r.ms);
                for (const Failure& f : r.failures) {
                    std::printf("        %s:%u  %s\n", f.file.c_str(), f.line, f.expression.c_str());
                    if (!f.detail.empty()) std::printf("          %s\n", f.detail.c_str());
                }
            }
        }
        results.push_back(std::move(r));
    }

    const f64 totalMs = total.ElapsedMillis();

    if (json) {
        const std::string j = ResultsToJson(results, passed, failed, totalMs);
        std::fwrite(j.data(), 1, j.size(), stdout);
        std::fputc('\n', stdout);
    } else {
        std::printf("\n  %u passed, %u failed  (%.1f ms)\n", passed, failed, totalMs);
    }
    return failed == 0 ? 0 : 1;
}

int Main(int argc, char** argv) {
    std::string filter;
    bool json = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--json") {
            json = true;
        } else if (StartsWith(arg, "--filter=")) {
            filter = std::string(arg.substr(9));
        } else if (arg == "--list") {
            for (const Registry::Entry& e : Registry::Instance().Entries()) {
                std::printf("%s\n", FullName(e).c_str());
            }
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            std::printf("usage: %s [--filter=<substring>] [--json] [--list]\n", argv[0]);
            return 0;
        }
    }

    // 테스트는 조용해야 한다. 로그를 켜두면 실패 출력이 묻힌다.
    Log::SetLevel(json ? LogLevel::Off : LogLevel::Error);
    Log::AddSink(MakeConsoleSink(true));

    return RunAll(filter, json);
}

} // namespace alice::test
