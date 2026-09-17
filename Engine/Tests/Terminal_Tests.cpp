// SPDX-License-Identifier: MIT
// Shell redraws and UTF-8 must survive arbitrary pipe boundaries and preserve the primary screen.
#include "TestFramework.h"
#include "AliceEditor/TerminalScreen.h"
using namespace alice;

ALICE_TEST(Terminal, CursorEraseAndChunkedUtf8) {
    editor::TerminalScreen screen(12, 4);
    screen.Feed("progress 10%\r\x1b[2Kready");
    ALICE_CHECK_STR(screen.Text(), "ready");
    screen.Feed("\x1b[2;3H\xED\x95");
    screen.Feed("\x9C!");
    ALICE_CHECK(screen.Cell(2, 1).character == U'한');
    ALICE_CHECK(screen.Cell(3, 1).continuation);
    ALICE_CHECK(screen.Cell(4, 1).character == U'!');
    ALICE_CHECK_EQ(screen.CursorX(), 5);
    screen.Feed("\x1b[1;1H\x1b[J");
    ALICE_CHECK_STR(screen.Text(), "");
}
ALICE_TEST(Terminal, ColorsAlternateScreenAndModes) {
    editor::TerminalScreen screen(12, 4);
    screen.Feed("shell\x1b[?1049h\x1b[38;2;12;34;56mAI\x1b[?25l\x1b[?2004h\x1b[?1h");
    ALICE_CHECK_STR(screen.Text(), "AI");
    ALICE_CHECK_EQ(screen.Cell(0, 0).foreground, 0x0C2238u);
    ALICE_CHECK(!screen.CursorVisible());
    ALICE_CHECK(screen.BracketedPaste());
    ALICE_CHECK(screen.ApplicationCursor());
    screen.Feed("\x1b[?1049l\x1b[?25h\x1b[0m!");
    ALICE_CHECK_STR(screen.Text(), "shell!");
    ALICE_CHECK(screen.CursorVisible());
}
ALICE_TEST(Terminal, ScrollRegionResizeAndReplies) {
    editor::TerminalScreen screen(8, 4);
    screen.Feed("header\r\none\r\ntwo\r\nfooter\x1b[2;3r\x1b[3;1H\nnew");
    ALICE_CHECK_STR(screen.Text(), "header\ntwo\nnew\nfooter");
    screen.Feed("\x1b[6n\x1b[c");
    ALICE_CHECK_STR(screen.TakeReplies(), "\x1b[3;4R\x1b[?1;2c");
    screen.Resize(5, 3);
    ALICE_CHECK_EQ(screen.Columns(), 5);
    ALICE_CHECK_EQ(screen.Rows(), 3);
    ALICE_CHECK_STR(screen.Text(), "heade\ntwo\nnew");
}
ALICE_TEST(Terminal, DelayedWrapCombiningAndIgnoredStrings) {
    editor::TerminalScreen screen(4, 3);
    screen.Feed("abcd\rX\x1b]0;hidden title\x07\x1bPignored\x1b\\");
    ALICE_CHECK_STR(screen.Text(), "Xbcd");
    screen.Feed("\x1b[2;1He\xCC\x81");
    ALICE_CHECK_EQ(screen.CursorX(), 1);
    ALICE_CHECK(screen.Cell(0, 1).combining == U"\u0301");
}
ALICE_TEST(Terminal, ScrollbackRetainsPrimaryLinesAndExcludesAlternateScreen) {
    editor::TerminalScreen screen(8, 2);
    screen.Feed("first\r\nsecond\r\nthird");
    ALICE_CHECK_EQ(screen.HistoryRows(), 1);
    ALICE_CHECK(screen.DisplayCell(0, 0, 1).character == U'f');
    ALICE_CHECK(screen.DisplayCell(0, 1, 1).character == U's');
    screen.Feed("\x1b[?1049ha\r\nb\r\nc");
    ALICE_CHECK_EQ(screen.HistoryRows(), 0);
    screen.Feed("\x1b[?1049l");
    ALICE_CHECK_EQ(screen.HistoryRows(), 1);
    for (int line = 0; line < 2100; ++line) screen.Feed("\r\nx");
    ALICE_CHECK_EQ(screen.HistoryRows(), 2000);
}
#ifdef _WIN32
#include "AliceEditor/TerminalSession.h"
#include <chrono>
#include <filesystem>
#include <thread>
ALICE_TEST(Terminal, ConPtyInteractiveShellRoundTripAndShutdown) {
    editor::TerminalSession session;
    const auto cwdBytes = std::filesystem::current_path().generic_u8string();
    const std::string cwd(reinterpret_cast<const char*>(cwdBytes.data()), cwdBytes.size());
    auto started = session.Start(cwd, cwd, 100, 24);
    ALICE_REQUIRE_MSG(started.IsOk(), started.IsErr() ? started.Error().ToLine() : "");
    ALICE_REQUIRE(session.Running());
    ALICE_REQUIRE(session.Send("Write-Output ('ALICE_'+'PTY_READY'); Write-Output ('TTY_' + [Console]::IsInputRedirected); Write-Output ('LINE_' + [bool](Get-Module PSReadLine))\r").IsOk());
    bool ready = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto text = session.Snapshot().Text();
        if (text.find("ALICE_PTY_READY") != std::string::npos && text.find("TTY_False") != std::string::npos && text.find("LINE_True") != std::string::npos) { ready = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ALICE_CHECK_MSG(ready, session.Snapshot().Text());
    ALICE_REQUIRE(session.Send("Write-Output ('EDIT_'+'PASSX')\x1b[D\x1b[D\x7f\x1b[F\r").IsOk());
    bool edited = false;
    const auto editDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < editDeadline) {
        if (session.Snapshot().Text().find("EDIT_PASS\n") != std::string::npos) { edited = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ALICE_CHECK_MSG(edited, session.Snapshot().Text());
    auto oversized = session.Send(std::string(1024 * 1024 + 1, 'x'));
    ALICE_REQUIRE(oversized.IsErr());
    ALICE_CHECK_STR(oversized.Error().code, "editor.terminal.input_full");
    ALICE_CHECK(oversized.Error().mark.Valid());
    ALICE_CHECK(!oversized.Error().hint.empty());
    ALICE_REQUIRE(session.Resize(72, 12).IsOk());
    ALICE_CHECK_EQ(session.Snapshot().Columns(), 72);
    ALICE_CHECK_EQ(session.Snapshot().Rows(), 12);
    // Teardown must drain the final frame even while a client is producing more output than a pipe can hold.
    ALICE_REQUIRE(session.Send("1..10000 | ForEach-Object { Write-Output ('stream-' + $_) }\r").IsOk());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto before = std::chrono::steady_clock::now();
    session.Stop();
    ALICE_CHECK(!session.Running());
    ALICE_CHECK(std::chrono::steady_clock::now() - before < std::chrono::seconds(5));
    auto stopped = session.Send("x");
    ALICE_REQUIRE(stopped.IsErr());
    ALICE_CHECK(!stopped.Error().hint.empty());
    ALICE_CHECK(!stopped.Error().file.empty());
    session.Stop();
}
ALICE_TEST(Terminal, StartFailureHasLocationAndRecoveryHint) {
    editor::TerminalSession session;
    auto result = session.Start("Z:/__alice_missing_terminal_project__", "", 80, 24);
    ALICE_REQUIRE(result.IsErr());
    ALICE_CHECK(!result.Error().hint.empty());
    ALICE_CHECK(result.Error().mark.Valid());
    ALICE_CHECK_STR(result.Error().file, "Z:/__alice_missing_terminal_project__");
    ALICE_CHECK(!session.Running());
}
#endif
