// SPDX-License-Identifier: MIT
#include "CommandLine.h"

#include "Foundation/FileSystem.h"
#include "Foundation/StringUtil.h"

#include <cstdio>

#if ALICE_PLATFORM_WINDOWS
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <io.h>
#  include <windows.h>
#else
#  include <unistd.h>
#endif

namespace alice::cli {
namespace {

bool StdoutIsTerminal() {
#if ALICE_PLATFORM_WINDOWS
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

void EnableAnsiOnWindows() {
#if ALICE_PLATFORM_WINDOWS
    HANDLE handle = ::GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (handle != INVALID_HANDLE_VALUE && ::GetConsoleMode(handle, &mode)) {
        ::SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    // 콘솔 출력 코드페이지를 UTF-8 로. 없으면 한글 진단이 깨져서 나온다.
    ::SetConsoleOutputCP(CP_UTF8);
#endif
}

std::string Wrap(std::string_view code, std::string_view text) {
    if (!Output().color) return std::string(text);
    std::string out;
    out.reserve(text.size() + 12);
    out += "\x1b[";
    out += code;
    out += 'm';
    out += text;
    out += "\x1b[0m";
    return out;
}

} // namespace

Args::Args(int argc, char** argv) {
    m_program = (argc > 0 && argv[0]) ? fs::FileStem(argv[0]) : std::string("alice");

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg.empty()) continue;

        if (StartsWith(arg, "--")) {
            std::string_view body = arg.substr(2);
            const usize eq = body.find('=');
            std::string name  = std::string(eq == std::string_view::npos ? body : body.substr(0, eq));
            std::string value = (eq == std::string_view::npos) ? "true"
                                                               : std::string(body.substr(eq + 1));
            m_flags[name] = std::move(value);
            m_flagOrder.push_back(std::move(name));
            continue;
        }
        if (arg.size() >= 2 && arg[0] == '-' && arg[1] != '-') {
            // 짧은 플래그. -q 같은 것만 쓴다. 묶어 쓰기(-qv)는 지원하지 않는다.
            std::string name(arg.substr(1));
            m_flags[name] = "true";
            m_flagOrder.push_back(std::move(name));
            continue;
        }

        if (m_command.empty()) m_command = std::string(arg);
        else                   m_positional.emplace_back(arg);
    }
}

bool Args::Has(std::string_view flag) const {
    return m_flags.find(std::string(flag)) != m_flags.end();
}

std::string Args::Get(std::string_view flag, std::string fallback) const {
    auto it = m_flags.find(std::string(flag));
    return it == m_flags.end() ? std::move(fallback) : it->second;
}

i64 Args::GetInt(std::string_view flag, i64 fallback) const {
    auto it = m_flags.find(std::string(flag));
    if (it == m_flags.end()) return fallback;
    i64 value = 0;
    return ParseI64(it->second, value) ? value : fallback;
}

OutputMode& Output() {
    static OutputMode mode = [] {
        OutputMode m;
        m.color = StdoutIsTerminal();
        if (m.color) EnableAnsiOnWindows();
        return m;
    }();
    return mode;
}

void Print(std::string_view text) {
    if (Output().json || Output().quiet) return;
    std::fwrite(text.data(), 1, text.size(), stdout);
}

void PrintLine(std::string_view text) {
    if (Output().json || Output().quiet) return;
    std::fwrite(text.data(), 1, text.size(), stdout);
    std::fputc('\n', stdout);
}

void PrintError(std::string_view text) {
    // 에러는 --quiet 여도 나간다. 조용히 실패하는 것이 가장 나쁘다.
    if (Output().json) return;
    std::fwrite(text.data(), 1, text.size(), stderr);
    std::fputc('\n', stderr);
}

void PrintJson(std::string_view json) {
    if (!Output().json) return;
    std::fwrite(json.data(), 1, json.size(), stdout);
    std::fputc('\n', stdout);
}

std::string Bold(std::string_view text)   { return Wrap("1", text); }
std::string Dim(std::string_view text)    { return Wrap("90", text); }
std::string Red(std::string_view text)    { return Wrap("31", text); }
std::string Yellow(std::string_view text) { return Wrap("33", text); }
std::string Green(std::string_view text)  { return Wrap("32", text); }
std::string Cyan(std::string_view text)   { return Wrap("36", text); }

} // namespace alice::cli
