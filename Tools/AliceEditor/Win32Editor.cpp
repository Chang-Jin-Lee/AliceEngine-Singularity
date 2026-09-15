// SPDX-License-Identifier: MIT
// Provide a directly launchable editing surface while keeping platform widgets out of the engine.
#include "AliceEditor/DocumentModel.h"
#include "AliceEditor/SceneViewport.h"
#include "Foundation/FileSystem.h"
#include "Foundation/StringUtil.h"
#include "Foundation/Log.h"
#include "Doc/Writer.h"
#include <windowsx.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <algorithm>
#include <charconv>
#include <cmath>

namespace alice::editor {
namespace {
constexpr COLORREF kPanel = RGB(32, 37, 45), kText = RGB(213, 222, 235), kMuted = RGB(145, 161, 182);
enum Id { OpenProject = 101, OpenFile, Save, Validate, Undo, Redo, Scene, Source, Add, Remove, Apply, Frame,
          Hierarchy = 201, Project, DocumentText, Console, Name = 301, Axis = 310 };
std::wstring Wide(std::string_view text) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<usize>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), n); return result;
}
std::string Utf8(std::wstring_view text) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<usize>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), n, nullptr, nullptr); return result;
}
std::wstring Read(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<usize>(length) + 1, L'\0'); GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<usize>(length)); return text;
}
std::wstring Lines(std::string_view text) {
    std::string result;
    for (const char c : text) { if (c == '\n') result += '\r'; if (c != '\r') result += c; }
    return Wide(result);
}
void Label(HDC dc, int x, int y, const wchar_t* text, COLORREF color = kMuted) {
    SetTextColor(dc, color); TextOutW(dc, x, y, text, static_cast<int>(wcslen(text)));
}
struct App {
    HWND window{}, hierarchy{}, project{}, source{}, console{}, name{};
    HWND fields[9]{};
    HFONT font{}, heading{}, mono{};
    HBRUSH panel = CreateSolidBrush(kPanel), input = CreateSolidBrush(RGB(24, 29, 36));
    DocumentModel model;
    Viewport viewport;
    std::string root, notice;
    std::vector<std::string> files;
    int selected = 0, width = 1400, height = 900;
    bool sourceMode = false, sourceDirty = false, syncing = false, inspectorDirty = false, smoke = false;
    bool dragging = false;
    POINT dragStart{};
    int exitCode = 0;
    RECT scene{};
    ~App() { DeleteObject(panel); DeleteObject(input); DeleteObject(font); DeleteObject(heading); DeleteObject(mono); }
    HWND Control(const wchar_t* type, const wchar_t* text, DWORD style, int id) {
        HWND control = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE); return control;
    }
    void Button(const wchar_t* text, int id) { Control(L"BUTTON", text, BS_OWNERDRAW | WS_TABSTOP, id); }
    void Move(int id, int x, int y, int w, int h) { MoveWindow(GetDlgItem(window, id), x, y, (std::max)(1, w), (std::max)(1, h), TRUE); }
    void Create() {
        font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        heading = CreateFontW(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        mono = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Consolas");
        Button(L"Open project", OpenProject); Button(L"Open file", OpenFile); Button(L"Save", Save);
        Button(L"Validate", Validate); Button(L"Undo", Undo); Button(L"Redo", Redo);
        Button(L"Scene", Scene); Button(L"Document", Source); Button(L"+ Actor", Add); Button(L"Delete", Remove);
        Button(L"Apply changes", Apply); Button(L"Frame origin", Frame);
        hierarchy = Control(L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP, Hierarchy);
        project = Control(L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP, Project);
        source = Control(L"EDIT", L"", ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP, DocumentText);
        console = Control(L"EDIT", L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, Console);
        name = Control(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, Name);
        for (int i = 0; i < 9; ++i) fields[i] = Control(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, Axis + i);
        SendMessageW(source, EM_SETLIMITTEXT, 16 * 1024 * 1024, 0);
        SendMessageW(source, WM_SETFONT, reinterpret_cast<WPARAM>(mono), FALSE);
        SendMessageW(console, WM_SETFONT, reinterpret_cast<WPARAM>(mono), FALSE);
        ListProject();
        std::string initial;
        for (const auto& file : files) if (file.ends_with("main.scene.yaml")) { initial = file; break; }
        if (initial.empty() && !files.empty()) initial = files.front();
        if (!initial.empty()) Open(initial); else { notice = "Open a project folder or a YAML/JSON document to begin."; Refresh(); }
        if (smoke) SetTimer(window, 1, 700, nullptr);
    }
    void Layout() {
        const int left = 236, right = width - 300, bottom = height - 174;
        int x = 16;
        for (const auto [id, size] : {std::pair{OpenProject, 112}, {OpenFile, 88}, {Save, 66}, {Validate, 88}, {Undo, 66}, {Redo, 66}}) {
            Move(id, x, 54, size, 30); x += size + 7;
        }
        Move(Scene, left + 16, 108, 72, 28); Move(Source, left + 95, 108, 94, 28); Move(Frame, right - 134, 108, 118, 28);
        Move(Hierarchy, 16, 145, left - 28, (bottom - 170) / 2);
        const int projectTop = 145 + (bottom - 170) / 2 + 34;
        Move(Project, 16, projectTop, left - 28, bottom - projectTop - 10);
        Move(Add, right + 16, 148, 104, 28); Move(Remove, right + 128, 148, 104, 28);
        Move(Name, right + 16, 211, 264, 27);
        for (int group = 0; group < 3; ++group) for (int axis = 0; axis < 3; ++axis)
            Move(Axis + group * 3 + axis, right + 16 + axis * 90, 292 + group * 79, 82, 26);
        Move(Apply, right + 16, 534, 264, 32);
        Move(Console, 16, bottom + 36, width - 32, 106);
        scene = {left + 8, 144, right - 8, bottom - 8};
        Move(DocumentText, scene.left + 8, scene.top + 8, scene.right - scene.left - 16, scene.bottom - scene.top - 16);
        ShowWindow(source, sourceMode ? SW_SHOW : SW_HIDE);
        InvalidateRect(window, nullptr, FALSE);
    }
    void ListProject() {
        files.clear(); SendMessageW(project, LB_RESETCONTENT, 0, 0);
        for (const char* ext : {".yaml", ".yml", ".json"}) {
            auto found = fs::ListFiles(root, ext, true); files.insert(files.end(), found.begin(), found.end());
        }
        std::sort(files.begin(), files.end());
        for (const auto& file : files) {
            auto display = file.starts_with(root + "/") ? file.substr(root.size() + 1) : fs::FileName(file);
            SendMessageW(project, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Wide(display).c_str()));
        }
    }
    void StatusText() {
        const bool dirty = model.IsDirty() || sourceDirty || inspectorDirty;
        const auto title = Wide((dirty ? "* " : "") + fs::FileName(model.Path()) + " — AliceEngine Singularity | Scene Editor");
        SetWindowTextW(window, title.c_str());
        const auto report = model.Diagnostics().ToPretty();
        const auto text = Lines(notice + "\n" + (report.empty() ? "Validation: no errors.\n" : report));
        SetWindowTextW(console, text.c_str()); InvalidateRect(window, nullptr, FALSE);
    }
    void Inspector() {
        syncing = true; inspectorDirty = false;
        const auto actors = model.Actors(); const bool valid = !sourceMode && selected >= 0 && static_cast<usize>(selected) < actors.size() && model.IsValid();
        EnableWindow(name, valid); EnableWindow(GetDlgItem(window, Apply), valid);
        EnableWindow(GetDlgItem(window, Remove), valid); EnableWindow(GetDlgItem(window, Add), !sourceMode && model.IsValid() && model.IsScene());
        SetWindowTextW(name, valid ? Wide(actors[static_cast<usize>(selected)].name).c_str() : L"");
        for (int i = 0; i < 9; ++i) {
            EnableWindow(fields[i], valid);
            std::wstring value;
            if (valid) {
                const auto& actor = actors[static_cast<usize>(selected)];
                const auto& vector = i < 3 ? actor.position : i < 6 ? actor.rotation : actor.scale;
                value = Wide(FormatDouble(vector[static_cast<usize>(i % 3)]));
            }
            SetWindowTextW(fields[i], value.c_str());
        }
        syncing = false;
    }
    void Refresh() {
        syncing = true; sourceDirty = false;
        SendMessageW(hierarchy, LB_RESETCONTENT, 0, 0);
        const auto actors = model.Actors();
        for (const auto& actor : actors) SendMessageW(hierarchy, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Wide(actor.name).c_str()));
        if (selected >= static_cast<int>(actors.size())) selected = static_cast<int>(actors.size()) - 1;
        if (selected < 0 && !actors.empty()) selected = 0;
        SendMessageW(hierarchy, LB_SETCURSEL, static_cast<WPARAM>(selected), 0);
        SetWindowTextW(source, Lines(model.Text()).c_str()); syncing = false;
        Inspector(); StatusText(); Layout();
    }
    bool ApplyInspector() {
        if (!inspectorDirty) return true;
        auto actors = model.Actors();
        if (selected < 0 || static_cast<usize>(selected) >= actors.size()) return false;
        auto actor = actors[static_cast<usize>(selected)]; actor.name = Utf8(Read(name));
        for (int i = 0; i < 9; ++i) {
            const auto ownedText = Utf8(Read(fields[i]));
            const auto text = Trim(ownedText);
            double number = 0;
            auto result = std::from_chars(text.data(), text.data() + text.size(), number);
            if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || !std::isfinite(number)) {
                auto error = MakeError("editor.transform.invalid_number", "Transform input is not a finite number",
                    "Enter a finite decimal number in this axis field, or use Undo to restore the applied value");
                const char* field = i < 3 ? "position" : i < 6 ? "rotation" : "scale";
                error.file = model.Path();
                error.path = "actors[" + std::to_string(selected) + "].transform." + field + "[" + std::to_string(i % 3) + "]";
                const auto* original = model.Document().root.AtPath(error.path);
                error.mark = original && original->mark.Valid() ? original->mark : Mark{1, 1, 0};
                notice = error.ToPretty(); StatusText(); return false;
            }
            auto& vector = i < 3 ? actor.position : i < 6 ? actor.rotation : actor.scale;
            vector[static_cast<usize>(i % 3)] = number;
        }
        const auto result = model.EditActor(static_cast<usize>(selected), actor);
        if (!result) { notice = result.Error().ToPretty(); StatusText(); return false; }
        inspectorDirty = false; return true;
    }
    bool FlushEdits() {
        if (sourceDirty) {
            auto text = Utf8(Read(source)); text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
            model.SetText(std::move(text)); sourceDirty = false;
        }
        return ApplyInspector();
    }
    bool SaveFile() {
        if (!FlushEdits()) return false;
        const auto result = model.Save();
        notice = result ? "Saved " + model.Path() : result.Error().ToPretty();
        if (result) ALICE_LOG_INFO("editor", "editor.document.saved").F("path", model.Path());
        Refresh(); return result.IsOk();
    }
    bool LeaveDocument() {
        if (!(model.IsDirty() || sourceDirty || inspectorDirty)) return true;
        const int answer = MessageBoxW(window, L"Save changes before continuing?", L"Unsaved document", MB_YESNOCANCEL | MB_ICONQUESTION);
        return answer == IDNO || (answer == IDYES && SaveFile());
    }
    void Open(const std::string& path) {
        const auto result = model.Open(path); notice = result ? "Opened " + path : result.Error().ToPretty();
        if (result) { selected = 0; sourceMode = !model.IsScene(); }
        Refresh();
    }
    void ChooseFile() {
        wchar_t path[32768]{}; OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = window; dialog.lpstrFilter = L"Content documents\0*.yaml;*.yml;*.json\0All files\0*.*\0";
        dialog.lpstrFile = path; dialog.nMaxFile = 32768; dialog.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&dialog) && LeaveDocument()) Open(Utf8(path));
    }
    void ChooseProject() {
        BROWSEINFOW dialog{}; dialog.hwndOwner = window; dialog.lpszTitle = L"Choose a content project folder";
        dialog.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&dialog);
        if (!item) return;
        wchar_t path[MAX_PATH]{}; const bool found = SHGetPathFromIDListW(item, path) != FALSE; CoTaskMemFree(item);
        if (!found || !LeaveDocument()) return;
        root = fs::Normalize(Utf8(path)); ListProject();
        if (!files.empty()) Open(files.front());
        else { notice = "This folder contains no YAML or JSON documents."; StatusText(); }
    }
    void Command(int id) {
        if (id == OpenProject) { ChooseProject(); return; }
        if (id == OpenFile) { ChooseFile(); return; }
        if (id == Save) { SaveFile(); return; }
        if (id == Frame) { viewport = {}; InvalidateRect(window, &scene, FALSE); return; }
        if (id == Undo && inspectorDirty) { Inspector(); notice = "Reverted pending inspector edits."; StatusText(); return; }
        if (!FlushEdits()) return;
        if (id == Undo) { model.Undo(); notice = "Undo"; }
        if (id == Redo) { model.Redo(); notice = "Redo"; }
        if (id == Validate || id == Apply) { model.Validate(); notice = "Checked " + model.Path(); }
        if (id == Scene) sourceMode = false;
        if (id == Source) sourceMode = true;
        if (id == Add || id == Remove) {
            const auto result = id == Add ? model.AddActor() : model.DeleteActor(static_cast<usize>(selected));
            notice = result ? "Scene changed. Save to write the document." : result.Error().ToPretty();
            if (result && id == Add) selected = static_cast<int>(model.Actors().size()) - 1;
        }
        Refresh();
    }
    void Paint() {
        PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint);
        RECT client{}; GetClientRect(window, &client);
        HDC buffer = CreateCompatibleDC(dc); HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height);
        HGDIOBJ oldBitmap = SelectObject(buffer, bitmap); FillRect(buffer, &client, panel);
        SetBkMode(buffer, TRANSPARENT); SelectObject(buffer, heading);
        Label(buffer, 16, 17, L"ALICE / SINGULARITY", kText);
        SelectObject(buffer, font);
        Label(buffer, 266, 20, L"SCENE EDITOR", RGB(106, 224, 176));
        Label(buffer, width - 278, 20, L"Document-driven workspace");
        Label(buffer, 16, 116, L"HIERARCHY / ROOT ACTORS");
        const int bottom = height - 174;
        Label(buffer, 16, 145 + (bottom - 170) / 2 + 9, L"PROJECT / DOCUMENTS");
        const int right = width - 300;
        Label(buffer, right + 16, 116, L"INSPECTOR"); Label(buffer, right + 16, 187, L"Name");
        for (int group = 0; group < 3; ++group) {
            Label(buffer, right + 16, 249 + group * 79, group == 0 ? L"Position" : group == 1 ? L"Rotation / degrees" : L"Scale");
            for (int axis = 0; axis < 3; ++axis) Label(buffer, right + 16 + axis * 90, 271 + group * 79, axis == 0 ? L"X" : axis == 1 ? L"Y" : L"Z");
        }
        Label(buffer, right + 16, 585, L"Components and nested actors:");
        Label(buffer, right + 16, 607, L"edit in the Document tab.");
        Label(buffer, 16, bottom + 10, L"CONSOLE / VALIDATION");
        Label(buffer, 16, height - 22, L"Ctrl+S Save    Ctrl+Z / Ctrl+Y Undo / Redo    F5 Validate");
        if (!sourceMode) {
            viewport.Paint(buffer, scene, model.IsValid() ? model.Actors() : std::vector<Actor>{}, selected);
            Label(buffer, scene.left + 14, scene.top + 12, L"SCENE LAYOUT / transform proxies");
            Label(buffer, scene.left + 14, scene.bottom - 28, L"Click a pivot to select  |  Wheel to zoom  |  Middle-drag to pan");
        }
        BitBlt(dc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, oldBitmap); DeleteObject(bitmap); DeleteDC(buffer); EndPaint(window, &paint);
    }
    void SmokeTest() {
        KillTimer(window, 1);
        bool ok = true;
        auto checks = doc::Value::MakeMap();
        auto check = [&](const char* key, bool passed) { checks.Set(key, doc::Value{passed}); ok = ok && passed; };
        check("window_and_scene", IsWindowVisible(window) && IsWindowVisible(hierarchy) && IsWindowVisible(name) && model.IsValid() && !model.Actors().empty());
        if (ok) {
            const auto before = model.Text();
            SetWindowTextW(fields[0], L"7.25"); Command(Apply);
            check("inspector_edit", model.Actors()[static_cast<usize>(selected)].position[0] == 7.25);
            Command(Undo); check("undo", model.Text() == before);
            SetWindowTextW(fields[0], L"invalid"); Command(Apply);
            check("invalid_axis_rejected", inspectorDirty && model.Text() == before);
            Command(Undo); check("invalid_axis_recovery", !inspectorDirty && model.Text() == before);
            Command(Source);
            check("source_isolates_inspector", IsWindowVisible(source) && !IsWindowEnabled(name) && !IsWindowEnabled(fields[0]));
            check("multiline_enter", (GetWindowLongPtrW(source, GWL_STYLE) & ES_WANTRETURN) != 0);
            SendMessageW(source, EM_SETSEL, 0, 0); SendMessageW(source, WM_CHAR, '\r', 0);
            check("source_edit", sourceDirty);
            Command(Undo); check("source_undo", model.Text() == before);
            Command(Scene); check("scene_tab", !IsWindowVisible(source) && IsWindowEnabled(name));
        }
        auto report = doc::Value::MakeMap(); report.Set("ok", doc::Value{ok}); report.Set("checks", std::move(checks));
        const auto written = fs::WriteTextFile(fs::Join(fs::ExecutableDirectory(), "editor-smoke.json"), doc::ToJson(report));
        ok = ok && written.IsOk();
        exitCode = ok ? 0 : 1;
        DestroyWindow(window);
    }
};
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        app->window = window; SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, message, wParam, lParam);
    switch (message) {
    case WM_CREATE: app->Create(); return 0;
    case WM_SIZE: app->width = LOWORD(lParam); app->height = HIWORD(lParam); if (wParam != SIZE_MINIMIZED) app->Layout(); return 0;
    case WM_GETMINMAXINFO: reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = {1100, 800}; return 0;
    case WM_PAINT: app->Paint(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_COMMAND: {
        const int id = LOWORD(wParam), code = HIWORD(wParam);
        if (!app->syncing && code == EN_CHANGE) {
            if (id == DocumentText) app->sourceDirty = true;
            if (id == Name || (id >= Axis && id < Axis + 9)) app->inspectorDirty = true;
            app->StatusText(); return 0;
        }
        if (id == Hierarchy && code == LBN_SELCHANGE) {
            const int next = static_cast<int>(SendMessageW(app->hierarchy, LB_GETCURSEL, 0, 0));
            if (app->FlushEdits()) { app->selected = next; app->Refresh(); }
            else SendMessageW(app->hierarchy, LB_SETCURSEL, static_cast<WPARAM>(app->selected), 0);
            return 0;
        }
        if (id == Project && code == LBN_DBLCLK) {
            const auto index = static_cast<usize>(SendMessageW(app->project, LB_GETCURSEL, 0, 0));
            if (index < app->files.size() && app->LeaveDocument()) app->Open(app->files[index]);
            return 0;
        }
        if (id >= OpenProject && id <= Frame) { app->Command(id); return 0; }
        break;
    }
    case WM_CTLCOLORLISTBOX: case WM_CTLCOLOREDIT: case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam); SetTextColor(dc, kText); SetBkColor(dc, RGB(24, 29, 36));
        return reinterpret_cast<LRESULT>(app->input);
    }
    case WM_DRAWITEM: {
        const auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (item->CtlType != ODT_BUTTON) break;
        const bool active = item->CtlID == Save || (item->CtlID == Source && app->sourceMode) || (item->CtlID == Scene && !app->sourceMode);
        HBRUSH brush = CreateSolidBrush(active ? RGB(43, 83, 76) : RGB(48, 57, 70));
        FillRect(item->hDC, &item->rcItem, brush); DeleteObject(brush);
        SetBkMode(item->hDC, TRANSPARENT); SetTextColor(item->hDC, item->itemState & ODS_DISABLED ? kMuted : kText);
        SelectObject(item->hDC, app->font); RECT text = item->rcItem; const auto label = Read(item->hwndItem);
        DrawTextW(item->hDC, label.c_str(), -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (item->itemState & ODS_FOCUS) DrawFocusRect(item->hDC, &text);
        return TRUE;
    }
    case WM_LBUTTONDOWN: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (!app->sourceMode && PtInRect(&app->scene, point) && app->FlushEdits()) {
            app->selected = app->viewport.Hit(app->scene, point, app->model.Actors()); app->Refresh();
        }
        return 0;
    }
    case WM_MBUTTONDOWN: app->dragging = true; app->dragStart = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}; SetCapture(window); return 0;
    case WM_MBUTTONUP: app->dragging = false; ReleaseCapture(); return 0;
    case WM_MOUSEMOVE:
        if (app->dragging) {
            POINT current{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            app->viewport.pan.x += current.x - app->dragStart.x; app->viewport.pan.y += current.y - app->dragStart.y;
            app->dragStart = current; InvalidateRect(window, &app->scene, FALSE);
        }
        return 0;
    case WM_MOUSEWHEEL: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}; ScreenToClient(window, &point);
        if (!app->sourceMode && PtInRect(&app->scene, point)) {
            app->viewport.zoom = std::clamp(app->viewport.zoom * (GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1.15 : 1 / 1.15), 1.0, 200.0);
            InvalidateRect(window, &app->scene, FALSE);
        }
        return 0;
    }
    case WM_TIMER: if (app->smoke) app->SmokeTest(); return 0;
    case WM_CLOSE: if (app->LeaveDocument()) DestroyWindow(window); return 0;
    case WM_DESTROY: PostQuitMessage(app->exitCode); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace
int Run(HINSTANCE instance, int show) {
    SetProcessDPIAware();
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    App app;
    int argc = 0; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv && i < argc; ++i) {
        const std::wstring argument = argv[i];
        if (argument == L"--smoke-test") app.smoke = true;
        else if (argument == L"--project" && i + 1 < argc) app.root = fs::Normalize(Utf8(argv[++i]));
    }
    if (argv) LocalFree(argv);
    if (app.root.empty()) {
        auto base = fs::ExecutableDirectory();
        for (int i = 0; i < 5; ++i) {
            const auto sample = fs::Join(base, "Samples/FirstLight");
            if (fs::IsDirectory(sample)) { app.root = sample; break; }
            base = fs::ParentPath(base);
        }
        if (app.root.empty()) app.root = fs::CurrentDirectory();
    }
    WNDCLASSW cls{}; cls.hInstance = instance; cls.lpfnWndProc = WindowProc;
    cls.lpszClassName = L"AliceSingularityEditor"; cls.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    cls.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512)); RegisterClassW(&cls);
    HWND window = CreateWindowExW(WS_EX_APPWINDOW, cls.lpszClassName, L"AliceEngine Singularity", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1440, 920, nullptr, nullptr, instance, &app);
    if (!window) { if (SUCCEEDED(com)) CoUninitialize(); return 1; }
    ShowWindow(window, show); UpdateWindow(window);
    const ACCEL shortcuts[] = {{FVIRTKEY | FCONTROL, 'S', Save}, {FVIRTKEY | FCONTROL, 'O', OpenFile},
        {FVIRTKEY | FCONTROL, 'Z', Undo}, {FVIRTKEY | FCONTROL, 'Y', Redo}, {FVIRTKEY, VK_F5, Validate}};
    HACCEL accelerator = CreateAcceleratorTableW(const_cast<ACCEL*>(shortcuts), static_cast<int>(std::size(shortcuts)));
    MSG message{}; int result = 0;
    while ((result = static_cast<int>(GetMessageW(&message, nullptr, 0, 0))) > 0) {
        if (!TranslateAcceleratorW(window, accelerator, &message) && !IsDialogMessageW(window, &message)) {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
    }
    DestroyAcceleratorTable(accelerator); if (SUCCEEDED(com)) CoUninitialize();
    return result < 0 ? 1 : static_cast<int>(message.wParam);
}
} // namespace alice::editor
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) { return alice::editor::Run(instance, show); }
