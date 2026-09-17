// SPDX-License-Identifier: MIT
// A bounded screen keeps shell redraws independent of the native renderer.
#include "AliceEditor/TerminalScreen.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <utility>
namespace alice::editor {
namespace {
constexpr std::array<std::uint32_t, 16> kPalette = {
    0x161A22, 0xCD3131, 0x0DBC79, 0xE5E510, 0x2472C8, 0xBC3FBC, 0x11A8CD, 0xD4D4D4,
    0x666666, 0xF14C4C, 0x23D18B, 0xF5F543, 0x3B8EEA, 0xD670D6, 0x29B8DB, 0xFFFFFF};
std::uint32_t Color(int index) {
    index = std::clamp(index, 0, 255);
    if (index < 16) return kPalette[static_cast<std::size_t>(index)];
    if (index >= 232) return static_cast<std::uint32_t>((8 + (index - 232) * 10) * 0x010101);
    index -= 16;
    const auto component = [](int n) { return n == 0 ? 0 : 55 + n * 40; };
    return static_cast<std::uint32_t>((component(index / 36) << 16) |
        (component((index / 6) % 6) << 8) | component(index % 6));
}
void Utf8(std::string& text, char32_t ch) {
    if (ch < 0x80) text += static_cast<char>(ch);
    else if (ch < 0x800) {
        text += static_cast<char>(0xC0 | (ch >> 6)); text += static_cast<char>(0x80 | (ch & 63));
    } else if (ch < 0x10000) {
        text += static_cast<char>(0xE0 | (ch >> 12)); text += static_cast<char>(0x80 | ((ch >> 6) & 63));
        text += static_cast<char>(0x80 | (ch & 63));
    } else {
        text += static_cast<char>(0xF0 | (ch >> 18)); text += static_cast<char>(0x80 | ((ch >> 12) & 63));
        text += static_cast<char>(0x80 | ((ch >> 6) & 63)); text += static_cast<char>(0x80 | (ch & 63));
    }
}
int Width(char32_t ch) {
    if ((ch >= 0x300 && ch <= 0x36F) || (ch >= 0xFE00 && ch <= 0xFE0F) || ch == 0x200D) return 0;
    if ((ch >= 0x1100 && ch <= 0x115F) || (ch >= 0x2E80 && ch <= 0xA4CF) ||
        (ch >= 0xAC00 && ch <= 0xD7A3) || (ch >= 0xF900 && ch <= 0xFAFF) ||
        (ch >= 0xFE10 && ch <= 0xFE6F) || (ch >= 0xFF01 && ch <= 0xFF60) ||
        (ch >= 0xFFE0 && ch <= 0xFFE6) || (ch >= 0x1F300 && ch <= 0x1FAFF) || ch >= 0x20000) return 2;
    return 1;
}
}
TerminalScreen::TerminalScreen(int columns, int rows) { Resize(columns, rows); }
void TerminalScreen::Resize(int columns, int rows) {
    ++m_revision;
    columns = std::clamp(columns, 2, 512); rows = std::clamp(rows, 1, 256);
    const auto resize = [&](std::vector<TerminalCell>& source) {
        std::vector<TerminalCell> target(static_cast<std::size_t>(columns * rows));
        for (int y = 0; y < std::min(rows, m_rows); ++y)
            for (int x = 0; x < std::min(columns, m_columns); ++x)
                target[static_cast<std::size_t>(y * columns + x)] = source[static_cast<std::size_t>(y * m_columns + x)];
        source = std::move(target);
    };
    resize(m_cells);
    if (!m_primary.empty()) resize(m_primary);
    m_columns = columns; m_rows = rows;
    m_x = std::min(m_x, columns - 1); m_y = std::min(m_y, rows - 1);
    m_top = 0; m_bottom = rows - 1; m_wrapPending = false;
}
TerminalCell TerminalScreen::Blank() const {
    TerminalCell cell;
    cell.foreground = m_inverse ? m_background : m_foreground;
    cell.background = m_inverse ? m_foreground : m_background;
    return cell;
}
void TerminalScreen::Erase(int first, int last) {
    first = std::clamp(first, 0, m_columns * m_rows);
    last = std::clamp(last, first, m_columns * m_rows);
    std::fill(m_cells.begin() + first, m_cells.begin() + last, Blank());
}
void TerminalScreen::Scroll(int amount) {
    const int count = std::min(std::abs(amount), m_bottom - m_top + 1) * m_columns;
    auto first = m_cells.begin() + m_top * m_columns;
    auto last = m_cells.begin() + (m_bottom + 1) * m_columns;
    if (amount > 0 && !m_alternate && m_top == 0 && m_bottom == m_rows - 1) {
        for (int offset = 0; offset < count; offset += m_columns) {
            m_history.push_back(std::make_shared<const std::vector<TerminalCell>>(first + offset, first + offset + m_columns));
            if (m_history.size() > 2000) m_history.pop_front();
        }
    }
    if (amount > 0) { std::move(first + count, last, first); std::fill(last - count, last, Blank()); }
    else { std::move_backward(first, last - count, last); std::fill(first, first + count, Blank()); }
}
void TerminalScreen::LineFeed() {
    m_wrapPending = false;
    if (m_y == m_bottom) Scroll(1);
    else m_y = std::min(m_y + 1, m_rows - 1);
}
void TerminalScreen::Put(char32_t ch) {
    const int width = Width(ch);
    if (width == 0) {
        int x = m_wrapPending ? m_x : m_x - 1;
        if (x >= 0 && Cell(x, m_y).continuation) --x;
        if (x >= 0) {
            auto& combining = m_cells[static_cast<std::size_t>(m_y * m_columns + x)].combining;
            if (combining.size() < 16) combining += ch;
        }
        return;
    }
    if (m_wrapPending || (width == 2 && m_x == m_columns - 1)) {
        if (m_autoWrap) { m_x = 0; LineFeed(); }
        m_wrapPending = false;
    }
    const int index = m_y * m_columns + m_x;
    if (Cell(m_x, m_y).continuation && m_x > 0) m_cells[static_cast<std::size_t>(index - 1)] = Blank();
    if (m_x + 1 < m_columns && Cell(m_x + 1, m_y).continuation) m_cells[static_cast<std::size_t>(index + 1)] = Blank();
    m_cells[static_cast<std::size_t>(index)] = Blank();
    m_cells[static_cast<std::size_t>(index)].character = ch;
    if (width == 2 && m_x + 1 < m_columns) {
        m_cells[static_cast<std::size_t>(index + 1)] = Blank();
        m_cells[static_cast<std::size_t>(index + 1)].continuation = true;
    }
    m_x += width;
    if (m_x >= m_columns) { m_x = m_columns - 1; m_wrapPending = m_autoWrap; }
}
void TerminalScreen::ExecuteCsi(char command) {
    std::vector<int> args(1, 0);
    const bool privateMode = !m_sequence.empty() && m_sequence.front() == '?';
    for (char ch : m_sequence) {
        if (ch >= '0' && ch <= '9') args.back() = std::min(100000, args.back() * 10 + ch - '0');
        else if (ch == ';' || ch == ':') args.push_back(0);
    }
    const auto value = [&](std::size_t index, int fallback = 1) {
        return index < args.size() && args[index] != 0 ? args[index] : fallback;
    };
    const int n = value(0), index = m_y * m_columns + m_x;
    switch (command) {
    case 'A': m_y = std::max(0, m_y - n); break;
    case 'B': case 'e': m_y = std::min(m_rows - 1, m_y + n); break;
    case 'C': case 'a': m_x = std::min(m_columns - 1, m_x + n); break;
    case 'D': m_x = std::max(0, m_x - n); break;
    case 'E': m_x = 0; m_y = std::min(m_rows - 1, m_y + n); break;
    case 'F': m_x = 0; m_y = std::max(0, m_y - n); break;
    case 'G': case '`': m_x = std::clamp(n - 1, 0, m_columns - 1); break;
    case 'd': m_y = std::clamp(n - 1, 0, m_rows - 1); break;
    case 'H': case 'f': m_y = std::clamp(n - 1, 0, m_rows - 1); m_x = std::clamp(value(1) - 1, 0, m_columns - 1); break;
    case 'J':
        if (args[0] == 0) Erase(index, m_rows * m_columns);
        else if (args[0] == 1) Erase(0, index + 1);
        else if (args[0] == 2 || args[0] == 3) Erase(0, m_rows * m_columns);
        break;
    case 'K':
        if (args[0] == 0) Erase(index, (m_y + 1) * m_columns);
        else if (args[0] == 1) Erase(m_y * m_columns, index + 1);
        else if (args[0] == 2) Erase(m_y * m_columns, (m_y + 1) * m_columns);
        break;
    case 'X': Erase(index, std::min(index + n, (m_y + 1) * m_columns)); break;
    case 'P': case '@': {
        const int count = std::min(n, m_columns - m_x);
        auto first = m_cells.begin() + index, last = m_cells.begin() + (m_y + 1) * m_columns;
        if (command == 'P') { std::move(first + count, last, first); std::fill(last - count, last, Blank()); }
        else { std::move_backward(first, last - count, last); std::fill(first, first + count, Blank()); }
        break;
    }
    case 'L': case 'M':
        if (m_y >= m_top && m_y <= m_bottom) {
            const int top = m_top; m_top = m_y; Scroll(command == 'M' ? n : -n); m_top = top;
        }
        break;
    case 'S': Scroll(n); break;
    case 'T': Scroll(-n); break;
    case 'r':
        if (!privateMode) { m_top = std::clamp(n - 1, 0, m_rows - 1); m_bottom = std::clamp(value(1, m_rows) - 1, m_top, m_rows - 1); m_x = m_y = 0; }
        break;
    case 's': m_savedX = m_x; m_savedY = m_y; break;
    case 'u': m_x = std::min(m_savedX, m_columns - 1); m_y = std::min(m_savedY, m_rows - 1); break;
    case 'n':
        if (args[0] == 6) m_replies += "\x1b[" + std::to_string(m_y + 1) + ";" + std::to_string(m_x + 1) + "R";
        else if (args[0] == 5) m_replies += "\x1b[0n";
        break;
    case 'c': m_replies += "\x1b[?1;2c"; break;
    case 'h': case 'l':
        if (privateMode) for (int mode : args) {
            const bool enable = command == 'h';
            if (mode == 25) m_cursorVisible = enable;
            else if (mode == 1) m_applicationCursor = enable;
            else if (mode == 7) m_autoWrap = enable;
            else if (mode == 2004) m_bracketedPaste = enable;
            else if ((mode == 1049 || mode == 47 || mode == 1047) && enable != m_alternate) {
                if (enable) {
                    m_primary = m_cells; m_primaryX = m_x; m_primaryY = m_y;
                    Erase(0, m_rows * m_columns); m_x = m_y = 0;
                } else {
                    m_cells = std::move(m_primary); m_x = std::min(m_primaryX, m_columns - 1); m_y = std::min(m_primaryY, m_rows - 1);
                }
                m_alternate = enable; m_top = 0; m_bottom = m_rows - 1;
            }
        }
        break;
    case 'm':
        for (std::size_t i = 0; i < args.size(); ++i) {
            const int code = args[i];
            if (code == 0) { m_foreground = 0xD4D4D4; m_background = 0x161A22; m_bold = m_inverse = false; }
            else if (code == 1) m_bold = true;
            else if (code == 22) m_bold = false;
            else if (code == 7) m_inverse = true;
            else if (code == 27) m_inverse = false;
            else if (code == 39) m_foreground = 0xD4D4D4;
            else if (code == 49) m_background = 0x161A22;
            else if (code >= 30 && code <= 37) m_foreground = Color(code - 30 + (m_bold ? 8 : 0));
            else if (code >= 40 && code <= 47) m_background = Color(code - 40);
            else if (code >= 90 && code <= 97) m_foreground = Color(code - 90 + 8);
            else if (code >= 100 && code <= 107) m_background = Color(code - 100 + 8);
            else if ((code == 38 || code == 48) && i + 2 < args.size()) {
                std::uint32_t color = 0;
                if (args[i + 1] == 5) { color = Color(args[i + 2]); i += 2; }
                else if (args[i + 1] == 2 && i + 4 < args.size()) {
                    color = static_cast<std::uint32_t>((std::clamp(args[i + 2], 0, 255) << 16) |
                        (std::clamp(args[i + 3], 0, 255) << 8) | std::clamp(args[i + 4], 0, 255));
                    i += 4;
                } else continue;
                (code == 38 ? m_foreground : m_background) = color;
            }
        }
        break;
    default: break;
    }
    if (command != 'm' && command != 'h' && command != 'l' && command != 'n' && command != 'c') m_wrapPending = false;
}
void TerminalScreen::Feed(std::string_view utf8) {
    ++m_revision;
    for (const unsigned char ch : utf8) {
        if (m_state == State::String || m_state == State::StringEscape) {
            if (ch == 7 || (m_state == State::StringEscape && ch == '\\')) m_state = State::Ground;
            else m_state = ch == 27 ? State::StringEscape : State::String;
            continue;
        }
        if (m_state == State::Charset) { m_state = State::Ground; continue; }
        if (m_state == State::Escape) {
            m_state = State::Ground;
            if (ch == '[') { m_state = State::Csi; m_sequence.clear(); }
            else if (ch == ']' || ch == 'P' || ch == '^' || ch == '_') m_state = State::String;
            else if (ch == '(' || ch == ')' || ch == '*' || ch == '+') m_state = State::Charset;
            else if (ch == '7') { m_savedX = m_x; m_savedY = m_y; }
            else if (ch == '8') { m_x = std::min(m_savedX, m_columns - 1); m_y = std::min(m_savedY, m_rows - 1); m_wrapPending = false; }
            else if (ch == 'D') LineFeed();
            else if (ch == 'E') { m_x = 0; LineFeed(); }
            else if (ch == 'M') { if (m_y == m_top) Scroll(-1); else m_y = std::max(0, m_y - 1); }
            else if (ch == 'c') { const auto revision = m_revision; *this = TerminalScreen(m_columns, m_rows); m_revision = revision; }
            continue;
        }
        if (m_state == State::Csi) {
            if (ch >= 0x40 && ch <= 0x7E) { ExecuteCsi(static_cast<char>(ch)); m_state = State::Ground; }
            else if (ch == 27) m_state = State::Escape;
            else if (m_sequence.size() < 256) m_sequence += static_cast<char>(ch);
            else m_state = State::Ground;
            continue;
        }
        if (m_utf8Remaining != 0) {
            if ((ch & 0xC0) == 0x80) {
                m_utf8 = (m_utf8 << 6) | (ch & 63);
                if (--m_utf8Remaining == 0) Put(m_utf8 < m_utf8Minimum || m_utf8 > 0x10FFFF ||
                    (m_utf8 >= 0xD800 && m_utf8 <= 0xDFFF) ? U'\uFFFD' : m_utf8);
                continue;
            }
            m_utf8Remaining = 0; Put(U'\uFFFD');
        }
        if (ch == 27) m_state = State::Escape;
        else if (ch == '\r') { m_x = 0; m_wrapPending = false; }
        else if (ch == '\n' || ch == '\v' || ch == '\f') LineFeed();
        else if (ch == '\b') { m_x = std::max(0, m_x - 1); m_wrapPending = false; }
        else if (ch == '\t') { m_x = std::min(m_columns - 1, (m_x / 8 + 1) * 8); m_wrapPending = false; }
        else if (ch >= 0x20 && ch < 0x7F) Put(ch);
        else if (ch >= 0xC2 && ch <= 0xF4) {
            m_utf8Remaining = ch < 0xE0 ? 1 : ch < 0xF0 ? 2 : 3;
            m_utf8Minimum = ch < 0xE0 ? 0x80 : ch < 0xF0 ? 0x800 : 0x10000;
            m_utf8 = ch & (m_utf8Remaining == 1 ? 31 : m_utf8Remaining == 2 ? 15 : 7);
        } else if (ch >= 0x80) Put(U'\uFFFD');
    }
}
const TerminalCell& TerminalScreen::Cell(int x, int y) const { return m_cells[static_cast<std::size_t>(y * m_columns + x)]; }
const TerminalCell& TerminalScreen::DisplayCell(int x, int y, int scrollOffset) const {
    scrollOffset = std::clamp(scrollOffset, 0, HistoryRows());
    const int line = HistoryRows() + y - scrollOffset;
    if (line >= HistoryRows()) return Cell(x, line - HistoryRows());
    const auto& history = *m_history[static_cast<std::size_t>(line)];
    static const TerminalCell blank;
    return static_cast<std::size_t>(x) < history.size() ? history[static_cast<std::size_t>(x)] : blank;
}
std::string TerminalScreen::Text() const {
    std::string result;
    for (int y = 0; y < m_rows; ++y) {
        std::string line;
        for (int x = 0; x < m_columns; ++x) {
            const auto& cell = Cell(x, y);
            if (cell.continuation) continue;
            Utf8(line, cell.character);
            for (char32_t ch : cell.combining) Utf8(line, ch);
        }
        while (!line.empty() && line.back() == ' ') line.pop_back();
        if (y != 0) result += '\n';
        result += line;
    }
    while (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}
std::string TerminalScreen::TakeReplies() { return std::exchange(m_replies, {}); }
}
