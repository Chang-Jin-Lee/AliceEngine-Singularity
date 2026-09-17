// SPDX-License-Identifier: MIT
// Interpret a terminal byte stream independently of windowing and pipe chunk boundaries.
#pragma once
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
namespace alice::editor {
struct TerminalCell {
    char32_t character = U' ';
    std::u32string combining;
    std::uint32_t foreground = 0xD4D4D4;
    std::uint32_t background = 0x161A22;
    bool continuation = false;
};
class TerminalScreen {
public:
    TerminalScreen(int columns = 80, int rows = 24);
    void Resize(int columns, int rows);
    void Feed(std::string_view utf8);
    int Columns() const { return m_columns; }
    int Rows() const { return m_rows; }
    int CursorX() const { return m_x; }
    int CursorY() const { return m_y; }
    bool CursorVisible() const { return m_cursorVisible; }
    bool BracketedPaste() const { return m_bracketedPaste; }
    bool ApplicationCursor() const { return m_applicationCursor; }
    std::uint64_t Revision() const { return m_revision; }
    const TerminalCell& Cell(int x, int y) const;
    int HistoryRows() const { return m_alternate ? 0 : static_cast<int>(m_history.size()); }
    const TerminalCell& DisplayCell(int x, int y, int scrollOffset) const;
    std::string Text() const;
    std::string TakeReplies();
private:
    enum class State { Ground, Escape, Csi, String, StringEscape, Charset };
    void Put(char32_t codepoint);
    void LineFeed();
    void Scroll(int amount);
    void ExecuteCsi(char command);
    void Erase(int first, int last);
    TerminalCell Blank() const;
    int m_columns = 0, m_rows = 0, m_x = 0, m_y = 0;
    int m_savedX = 0, m_savedY = 0, m_top = 0, m_bottom = 0;
    int m_primaryX = 0, m_primaryY = 0;
    bool m_cursorVisible = true, m_bracketedPaste = false, m_applicationCursor = false;
    bool m_alternate = false, m_wrapPending = false, m_autoWrap = true, m_bold = false, m_inverse = false;
    std::uint32_t m_foreground = 0xD4D4D4, m_background = 0x161A22;
    std::vector<TerminalCell> m_cells, m_primary;
    std::deque<std::shared_ptr<const std::vector<TerminalCell>>> m_history;
    State m_state = State::Ground;
    std::string m_sequence, m_replies;
    char32_t m_utf8 = 0;
    int m_utf8Remaining = 0;
    char32_t m_utf8Minimum = 0;
    std::uint64_t m_revision = 0;
};
}
