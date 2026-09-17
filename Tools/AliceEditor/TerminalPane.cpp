// SPDX-License-Identifier: MIT
// Render the shell's terminal cells and translate native input without borrowing the editor's command dispatch.
#include "AliceEditor/TerminalPane.h"
#ifdef _WIN32
#include "AliceEditor/TerminalSession.h"
#include <imm.h>
#include <windowsx.h>
#include <algorithm>
#include <cstring>
#include <utility>
namespace alice::editor {
namespace {
constexpr int kPadding = 8, kHeader = 30, kFooter = 24;
constexpr wchar_t kClass[] = L"Alice.InteractiveTerminal";
std::wstring Wide(std::string_view text) {
    const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    if (count) MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), count);
    return result;
}
std::string Utf8(std::wstring_view text) {
    const int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(count), '\0');
    if (count) WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), count, nullptr, nullptr);
    return result;
}
void AppendWide(std::wstring& result, char32_t ch) {
    if (ch <= 0xFFFF) result += static_cast<wchar_t>(ch);
    else { ch -= 0x10000; result += static_cast<wchar_t>(0xD800 + (ch >> 10)); result += static_cast<wchar_t>(0xDC00 + (ch & 1023)); }
}
COLORREF Color(std::uint32_t rgb) { return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255); }
}
struct TerminalPane::Impl {
    TerminalSession session;
    TerminalScreen screen;
    HWND window = nullptr;
    HFONT font = nullptr, labelFont = nullptr;
    int cellWidth = 8, cellHeight = 18;
    int anchor = -1, selectedEnd = -1;
    int scrollOffset = 0, wheelRemainder = 0;
    bool dragging = false, focused = false, running = false;
    wchar_t highSurrogate = 0;
    std::string directory, failure;
    void ShowFailure(const Status& status) {
        if (status.IsErr()) { failure = status.Error().message + ". " + status.Error().hint; InvalidateRect(window, nullptr, FALSE); }
    }
    void Send(std::string_view bytes) {
        scrollOffset = 0; anchor = selectedEnd = -1;
        ShowFailure(session.Send(bytes)); InvalidateRect(window, nullptr, FALSE);
    }
    void SendWide(std::wstring_view text) { Send(Utf8(text)); }
    int Position(int x, int y) const {
        return std::clamp((y - kHeader) / cellHeight, 0, screen.Rows() - 1) * screen.Columns() +
            std::clamp((x - kPadding) / cellWidth, 0, screen.Columns() - 1);
    }
    void Layout() {
        if (!window) return;
        RECT rect{}; GetClientRect(window, &rect);
        const int columns = std::clamp((static_cast<int>(rect.right) - 2 * kPadding) / cellWidth, 2, 512);
        const int rows = std::clamp((static_cast<int>(rect.bottom) - kHeader - kFooter) / cellHeight, 1, 256);
        if (columns != screen.Columns() || rows != screen.Rows()) {
            if (session.Running()) ShowFailure(session.Resize(columns, rows));
            screen = session.Snapshot(); anchor = selectedEnd = -1;
        }
        MoveCaret(); InvalidateRect(window, nullptr, FALSE);
    }
    void MoveCaret() {
        if (!focused) return;
        SetCaretPos(kPadding + screen.CursorX() * cellWidth, kHeader + screen.CursorY() * cellHeight);
        if (auto ime = ImmGetContext(window)) {
            COMPOSITIONFORM form{}; form.dwStyle = CFS_POINT;
            form.ptCurrentPos = POINT{kPadding + screen.CursorX() * cellWidth, kHeader + screen.CursorY() * cellHeight};
            ImmSetCompositionWindow(ime, &form); ImmSetCompositionFontW(ime, &FontDescription());
            ImmReleaseContext(window, ime);
        }
    }
    LOGFONTW& FontDescription() {
        static thread_local LOGFONTW description{};
        GetObjectW(font, sizeof(description), &description); return description;
    }
    void Copy() {
        std::wstring text;
        if (anchor >= 0 && selectedEnd >= 0 && anchor != selectedEnd) {
            const int first = std::min(anchor, selectedEnd), last = std::max(anchor, selectedEnd);
            for (int index = first; index <= last; ++index) {
                if (index != first && index % screen.Columns() == 0) text += L"\r\n";
                const auto& cell = screen.DisplayCell(index % screen.Columns(), index / screen.Columns(), scrollOffset);
                if (!cell.continuation) { AppendWide(text, cell.character); for (char32_t ch : cell.combining) AppendWide(text, ch); }
            }
        } else {
            for (int y = 0; y < screen.Rows(); ++y) {
                std::wstring line;
                for (int x = 0; x < screen.Columns(); ++x) {
                    const auto& cell = screen.DisplayCell(x, y, scrollOffset);
                    if (!cell.continuation) { AppendWide(line, cell.character); for (char32_t ch : cell.combining) AppendWide(line, ch); }
                }
                while (!line.empty() && line.back() == L' ') line.pop_back();
                if (y != 0) text += L"\r\n";
                text += line;
            }
            while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n')) text.pop_back();
        }
        if (!OpenClipboard(window)) return;
        const auto memory = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
        if (memory) {
            if (auto data = GlobalLock(memory)) {
                memcpy(data, text.c_str(), (text.size() + 1) * sizeof(wchar_t)); GlobalUnlock(memory);
                EmptyClipboard();
                if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
            } else GlobalFree(memory);
        }
        CloseClipboard();
    }
    void Paste() {
        if (!OpenClipboard(window)) return;
        std::wstring clipboard;
        if (const auto memory = GetClipboardData(CF_UNICODETEXT)) {
            if (const auto data = static_cast<const wchar_t*>(GlobalLock(memory))) {
                const auto maximum = std::min<std::size_t>(GlobalSize(memory) / sizeof(wchar_t), 512 * 1024);
                std::size_t length = 0; while (length < maximum && data[length]) ++length;
                clipboard.assign(data, length); GlobalUnlock(memory);
            }
        }
        CloseClipboard();
        std::string bytes = Utf8(clipboard), normalized;
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            if (bytes[i] == '\r') { normalized += '\r'; if (i + 1 < bytes.size() && bytes[i + 1] == '\n') ++i; }
            else normalized += bytes[i] == '\n' ? '\r' : bytes[i];
        }
        if (screen.BracketedPaste()) normalized = "\x1b[200~" + normalized + "\x1b[201~";
        Send(normalized);
    }
    bool Key(WPARAM key) {
        const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        if (shift && (key == VK_PRIOR || key == VK_NEXT)) {
            scrollOffset = std::clamp(scrollOffset + (key == VK_PRIOR ? 1 : -1) * screen.Rows(), 0, screen.HistoryRows());
            anchor = selectedEnd = -1; InvalidateRect(window, nullptr, FALSE); return true;
        }
        if (control && shift && key == 'C') { Copy(); return true; }
        if ((control && key == 'V') || (shift && key == VK_INSERT)) { Paste(); return true; }
        if (control && key == VK_SPACE) { Send(std::string_view("\0", 1)); return true; }
        const int modifier = 1 + (shift ? 1 : 0) + (alt ? 2 : 0) + (control ? 4 : 0);
        const char* final = nullptr; int tilde = 0;
        switch (key) {
        case VK_UP: final = "A"; break;
        case VK_DOWN: final = "B"; break;
        case VK_RIGHT: final = "C"; break;
        case VK_LEFT: final = "D"; break;
        case VK_HOME: final = "H"; break;
        case VK_END: final = "F"; break;
        case VK_INSERT: tilde = 2; break;
        case VK_DELETE: tilde = 3; break;
        case VK_PRIOR: tilde = 5; break;
        case VK_NEXT: tilde = 6; break;
        case VK_F1: final = "P"; break;
        case VK_F2: final = "Q"; break;
        case VK_F3: final = "R"; break;
        case VK_F4: final = "S"; break;
        case VK_F5: tilde = 15; break;
        case VK_F6: tilde = 17; break;
        case VK_F7: tilde = 18; break;
        case VK_F8: tilde = 19; break;
        case VK_F9: tilde = 20; break;
        case VK_F10: tilde = 21; break;
        case VK_F11: tilde = 23; break;
        case VK_F12: tilde = 24; break;
        case VK_TAB: if (shift) { Send("\x1b[Z"); return true; } return false;
        case VK_ESCAPE: Send("\x1b"); return true;
        default: return false;
        }
        anchor = selectedEnd = -1;
        if (final) {
            if (modifier != 1) Send("\x1b[1;" + std::to_string(modifier) + final);
            else Send(std::string((screen.ApplicationCursor() || (key >= VK_F1 && key <= VK_F4)) ? "\x1bO" : "\x1b[") + final);
        } else Send("\x1b[" + std::to_string(tilde) + (modifier == 1 ? "" : ";" + std::to_string(modifier)) + "~");
        return true;
    }
    void Paint() {
        PAINTSTRUCT paint{}; const auto target = BeginPaint(window, &paint);
        RECT rect{}; GetClientRect(window, &rect);
        const auto dc = CreateCompatibleDC(target);
        const auto bitmap = CreateCompatibleBitmap(target, std::max(1L, rect.right), std::max(1L, rect.bottom));
        const auto oldBitmap = SelectObject(dc, bitmap);
        SetDCBrushColor(dc, RGB(22, 26, 34)); FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        const auto oldFont = SelectObject(dc, labelFont);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(140, 196, 213));
        RECT label{kPadding, 5, rect.right - kPadding, kHeader};
        DrawTextW(dc, L"TERMINAL  /  PowerShell", -1, &label, DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dc, font);
        for (int y = 0; y < screen.Rows(); ++y) for (int x = 0; x < screen.Columns(); ++x) {
            const auto& cell = screen.DisplayCell(x, y, scrollOffset);
            if (cell.continuation) continue;
            const int index = y * screen.Columns() + x;
            const bool selected = anchor >= 0 && selectedEnd >= 0 && anchor != selectedEnd &&
                index >= std::min(anchor, selectedEnd) && index <= std::max(anchor, selectedEnd);
            const int width = x + 1 < screen.Columns() && screen.DisplayCell(x + 1, y, scrollOffset).continuation ? 2 : 1;
            RECT cellRect{kPadding + x * cellWidth, kHeader + y * cellHeight,
                kPadding + (x + width) * cellWidth, kHeader + (y + 1) * cellHeight};
            SetTextColor(dc, Color(cell.foreground)); SetBkColor(dc, selected ? RGB(48, 74, 100) : Color(cell.background));
            std::wstring text; AppendWide(text, cell.character); for (char32_t ch : cell.combining) AppendWide(text, ch);
            ExtTextOutW(dc, cellRect.left, cellRect.top, ETO_OPAQUE | ETO_CLIPPED, &cellRect, text.c_str(), static_cast<UINT>(text.size()), nullptr);
        }
        if (focused && scrollOffset == 0 && screen.CursorVisible()) {
            RECT cursor{kPadding + screen.CursorX() * cellWidth, kHeader + (screen.CursorY() + 1) * cellHeight - 2,
                kPadding + (screen.CursorX() + 1) * cellWidth, kHeader + (screen.CursorY() + 1) * cellHeight};
            SetDCBrushColor(dc, RGB(140, 196, 213)); FillRect(dc, &cursor, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        }
        SelectObject(dc, labelFont); SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, failure.empty() ? RGB(130, 142, 160) : RGB(240, 142, 110));
        RECT footer{kPadding, rect.bottom - kFooter + 4, rect.right - kPadding, rect.bottom};
        const auto help = Wide(!failure.empty() ? failure : scrollOffset > 0 ? "Scrollback  |  Shift+PgDn returns toward live output" :
            running ? "Ctrl+Shift+C copy  |  Ctrl+V paste  |  Wheel scroll" : "Shell exited. Reopen the editor to restart.");
        DrawTextW(dc, help.c_str(), -1, &footer, DT_SINGLELINE | DT_END_ELLIPSIS);
        BitBlt(target, 0, 0, rect.right, rect.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldFont); SelectObject(dc, oldBitmap); DeleteObject(bitmap); DeleteDC(dc); EndPaint(window, &paint);
    }
    static LRESULT CALLBACK Procedure(HWND handle, UINT message, WPARAM wparam, LPARAM lparam) {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
            self->window = handle; SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(handle, message, wparam, lparam);
        switch (message) {
        case WM_GETDLGCODE: return DLGC_WANTALLKEYS | DLGC_WANTCHARS | DLGC_WANTARROWS | DLGC_WANTTAB;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: self->Paint(); return 0;
        case WM_SIZE: self->Layout(); return 0;
        case WM_SETFOCUS:
            self->focused = true; CreateCaret(handle, nullptr, 1, self->cellHeight); self->MoveCaret(); InvalidateRect(handle, nullptr, FALSE); return 0;
        case WM_KILLFOCUS: self->focused = false; DestroyCaret(); InvalidateRect(handle, nullptr, FALSE); return 0;
        case WM_TIMER: {
            auto next = self->session.Snapshot(); const bool running = self->session.Running();
            if (next.Revision() != self->screen.Revision() || running != self->running) {
                self->scrollOffset = std::clamp(self->scrollOffset, 0, next.HistoryRows());
                self->screen = std::move(next); self->running = running; self->MoveCaret(); InvalidateRect(handle, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDOWN:
            SetFocus(handle); SetCapture(handle); self->dragging = true;
            self->anchor = self->selectedEnd = self->Position(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)); return 0;
        case WM_MOUSEMOVE:
            if (self->dragging) { self->selectedEnd = self->Position(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)); InvalidateRect(handle, nullptr, FALSE); }
            return 0;
        case WM_LBUTTONUP: self->dragging = false; ReleaseCapture(); return 0;
        case WM_CAPTURECHANGED: self->dragging = false; return 0;
        case WM_MOUSEWHEEL:
            self->wheelRemainder += GET_WHEEL_DELTA_WPARAM(wparam);
            self->scrollOffset = std::clamp(self->scrollOffset + self->wheelRemainder / WHEEL_DELTA * 3, 0, self->screen.HistoryRows());
            self->wheelRemainder %= WHEEL_DELTA;
            self->anchor = self->selectedEnd = -1; InvalidateRect(handle, nullptr, FALSE); return 0;
        case WM_RBUTTONUP: SetFocus(handle); self->Paste(); return 0;
        case WM_KEYDOWN: case WM_SYSKEYDOWN: if (self->Key(wparam)) return 0; break;
        case WM_CHAR: {
            const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0, shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (wparam == 27 || (wparam == '\t' && shift) || (control && (wparam == 22 || (shift && wparam == 3))) || (control && wparam == 0)) return 0;
            // ConPTY maps DEL to Backspace; BS represents Ctrl+Backspace (delete a word).
            if (wparam == '\b' && !control) { self->Send("\x7f"); return 0; }
            const wchar_t ch = static_cast<wchar_t>(wparam);
            if (ch >= 0xD800 && ch <= 0xDBFF) { self->highSurrogate = ch; return 0; }
            std::wstring text;
            if (self->highSurrogate) { text += self->highSurrogate; self->highSurrogate = 0; }
            text += ch; self->anchor = self->selectedEnd = -1; self->SendWide(text); return 0;
        }
        case WM_SYSCHAR:
            if (wparam != ' ') { self->Send("\x1b" + Utf8(std::wstring(1, static_cast<wchar_t>(wparam)))); return 0; }
            break;
        case WM_IME_COMPOSITION:
            if (lparam & GCS_RESULTSTR) {
                if (auto ime = ImmGetContext(handle)) {
                    const LONG size = ImmGetCompositionStringW(ime, GCS_RESULTSTR, nullptr, 0);
                    if (size > 0) {
                        std::wstring text(static_cast<std::size_t>(size) / sizeof(wchar_t), L'\0');
                        ImmGetCompositionStringW(ime, GCS_RESULTSTR, text.data(), static_cast<DWORD>(size)); self->SendWide(text);
                    }
                    ImmReleaseContext(handle, ime);
                }
                return 0;
            }
            break;
        case WM_NCDESTROY:
            KillTimer(handle, 1); self->session.Stop(); self->window = nullptr;
            SetWindowLongPtrW(handle, GWLP_USERDATA, 0); break;
        default: break;
        }
        return DefWindowProcW(handle, message, wparam, lparam);
    }
};
TerminalPane::TerminalPane() : m_impl(std::make_unique<Impl>()) {}
TerminalPane::~TerminalPane() {
    Stop();
    if (m_impl->window) DestroyWindow(m_impl->window);
    if (m_impl->font) DeleteObject(m_impl->font);
    if (m_impl->labelFont) DeleteObject(m_impl->labelFont);
}
Status TerminalPane::Create(HWND parent, HINSTANCE instance, const std::string& projectDirectory, const std::string& engineBinDirectory) {
    auto& state = *m_impl; state.directory = projectDirectory;
    WNDCLASSEXW windowClass{}; windowClass.cbSize = sizeof(windowClass); windowClass.hInstance = instance;
    windowClass.lpfnWndProc = Impl::Procedure; windowClass.lpszClassName = kClass; windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32513));
    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        auto diagnostic = MakeError("editor.terminal.window", "cannot register the terminal window", "Close unused windows and reopen the editor"); diagnostic.file = projectDirectory; diagnostic.path = "terminal"; diagnostic.mark = {1, 1, 0}; return diagnostic;
    }
    state.font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    state.labelFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    const auto dc = GetDC(parent);
    const auto previous = SelectObject(dc, state.font);
    TEXTMETRICW metrics{}; GetTextMetricsW(dc, &metrics);
    state.cellWidth = std::max(1L, metrics.tmAveCharWidth); state.cellHeight = std::max(1L, metrics.tmHeight + 1);
    SelectObject(dc, previous); ReleaseDC(parent, dc);
    state.window = CreateWindowExW(0, kClass, L"Interactive PowerShell terminal", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 640, 480, parent, nullptr, instance, &state);
    if (!state.window) {
        auto diagnostic = MakeError("editor.terminal.window", "cannot create the terminal window", "Close unused windows and reopen the editor"); diagnostic.file = projectDirectory; diagnostic.path = "terminal"; diagnostic.mark = {1, 1, 0}; return diagnostic;
    }
    const auto started = state.session.Start(projectDirectory, engineBinDirectory, 80, 24);
    state.ShowFailure(started); state.running = state.session.Running(); state.Layout();
    SetTimer(state.window, 1, 40, nullptr);
    return started;
}
HWND TerminalPane::Handle() const { return m_impl->window; }
void TerminalPane::Resize(int x, int y, int width, int height) { if (m_impl->window) MoveWindow(m_impl->window, x, y, std::max(1, width), std::max(1, height), TRUE); }
Status TerminalPane::Send(std::string_view utf8) { return m_impl->session.Send(utf8); }
std::string TerminalPane::ScreenText() const { return m_impl->session.Snapshot().Text(); }
bool TerminalPane::Running() const { return m_impl->session.Running(); }
void TerminalPane::Stop() { m_impl->session.Stop(); }
}
#endif
