// SPDX-License-Identifier: MIT
// Provide a directly launchable editing surface while keeping platform widgets out of the engine.
#include "AliceEditor/DocumentModel.h"
#include "AliceEditor/SceneViewport.h"
#include "AliceEditor/WorkspaceLayout.h"
#include "AliceEditor/PlaySession.h"
#include "AliceEditor/TerminalPane.h"
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
#include <chrono>

namespace alice::editor {
namespace {
constexpr COLORREF kPanel = RGB(32, 37, 45), kText = RGB(213, 222, 235), kMuted = RGB(145, 161, 182);
enum Id { OpenProject = 101, OpenFile, Save, Validate, Undo, Redo, Scene, Source, Add, Remove, Apply, Frame,
          NewScene, Reload, Play, Stop, Demo,
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
    WorkspaceLayout workspace;
    PlaySession play;
    TerminalPane terminal;
    Split split = Split::None;
    std::chrono::steady_clock::time_point lastTick;
    std::string root, notice;
    std::vector<std::string> files;
    int selected = 0, width = 1400, height = 900;
    bool sourceMode = false, sourceDirty = false, syncing = false, inspectorDirty = false, smoke = false;
    bool dragging = false;
    POINT dragStart{};
    int exitCode = 0;
    int smokeWaits = 0;
    RECT scene{};
    std::vector<Actor> Actors() const { return play.IsPlaying() ? play.Actors() : model.Actors(); }
    bool TerminalFocus() const { const HWND focus = GetFocus(); return focus && (focus == terminal.Handle() || IsChild(terminal.Handle(), focus)); }
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
        Button(L"New scene", NewScene); Button(L"Reload", Reload); Button(L"Play", Play); Button(L"Stop", Stop); Button(L"Cube demo", Demo);
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
        const auto started = terminal.Create(window, GetModuleHandleW(nullptr), root, fs::ExecutableDirectory());
        if (!started) { notice = started.Error().ToPretty(); StatusText(); }
        Layout();
        if (smoke) {
            const auto sent = terminal.Send("Write-Output ('ALICE_'+'EDITOR_READY')\r");
            if (!sent) { notice = sent.Error().ToPretty(); StatusText(); }
            SetTimer(window, 1, 700, nullptr);
        }
    }
    void Layout() {
        const auto geometry = workspace.Compute(width, height);
        const int left = geometry.left, right = geometry.right, bottom = geometry.bottom, editor = geometry.editor;
        int x = 16;
        for (const auto [id, size] : {std::pair{NewScene, 88}, {OpenProject, 108}, {OpenFile, 82}, {Save, 54}, {Validate, 74}, {Undo, 54}, {Redo, 54}, {Reload, 66}, {Play, 54}, {Stop, 54}, {Demo, 94}, {Frame, 108}}) {
            Move(id, x, 54, size, 30); x += size + 6;
        }
        Move(Scene, left + 16, 108, 72, 28); Move(Source, left + 95, 108, 94, 28);
        const int projectTop = geometry.projectTop;
        Move(Hierarchy, 16, 145, left - 28, projectTop - 181);
        Move(Project, 16, projectTop, left - 28, bottom - projectTop - 10);
        const int inspectorWidth = editor - right - 32, axisWidth = inspectorWidth / 3;
        Move(Add, right + 16, 148, inspectorWidth / 2 - 4, 28); Move(Remove, right + 20 + inspectorWidth / 2, 148, inspectorWidth / 2 - 4, 28);
        Move(Name, right + 16, 211, inspectorWidth, 27);
        for (int group = 0; group < 3; ++group) for (int axis = 0; axis < 3; ++axis)
            Move(Axis + group * 3 + axis, right + 16 + axis * axisWidth, 292 + group * 79, axisWidth - 8, 26);
        Move(Apply, right + 16, 534, inspectorWidth, 32);
        Move(Console, 16, bottom + 36, editor - 32, height - bottom - 64);
        terminal.Resize(editor + 8, 144, width - editor - 16, height - 172);
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
        const auto actors = Actors(); const bool valid = !sourceMode && selected >= 0 && static_cast<usize>(selected) < actors.size() && model.IsValid();
        EnableWindow(name, valid); EnableWindow(GetDlgItem(window, Apply), valid);
        EnableWindow(GetDlgItem(window, Remove), valid && !play.IsPlaying()); EnableWindow(GetDlgItem(window, Add), !play.IsPlaying() && !sourceMode && model.IsValid() && model.IsScene());
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
        const auto actors = Actors();
        for (const auto& actor : actors) SendMessageW(hierarchy, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Wide(actor.name).c_str()));
        if (selected >= static_cast<int>(actors.size())) selected = static_cast<int>(actors.size()) - 1;
        if (selected < 0 && !actors.empty()) selected = 0;
        SendMessageW(hierarchy, LB_SETCURSEL, static_cast<WPARAM>(selected), 0);
        SetWindowTextW(source, Lines(model.Text()).c_str()); syncing = false;
        Inspector(); StatusText(); Layout();
        for (int id : {OpenProject, OpenFile, Save, Undo, Redo, Source, NewScene, Reload, Demo}) EnableWindow(GetDlgItem(window, id), !play.IsPlaying());
        EnableWindow(project, !play.IsPlaying());
        EnableWindow(GetDlgItem(window, Play), !play.IsPlaying() && model.IsScene() && model.IsValid());
        EnableWindow(GetDlgItem(window, Stop), play.IsPlaying());
    }
    bool ApplyInspector() {
        if (!inspectorDirty) return true;
        auto actors = Actors();
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
        const auto result = play.IsPlaying() ? play.EditActor(static_cast<usize>(selected), actor) : model.EditActor(static_cast<usize>(selected), actor);
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
        if (play.IsPlaying()) { play.Stop(); KillTimer(window, 2); inspectorDirty = false; Refresh(); }
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
    void CreateScene() {
        wchar_t path[32768] = L"NewScene.scene.yaml";
        const auto directory = Wide(fs::IsDirectory(fs::Join(root, "scenes")) ? fs::Join(root, "scenes") : root);
        OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = window;
        dialog.lpstrFilter = L"Scene documents\0*.scene.yaml\0"; dialog.lpstrDefExt = L"yaml";
        dialog.lpstrInitialDir = directory.c_str(); dialog.lpstrFile = path; dialog.nMaxFile = 32768;
        dialog.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&dialog) || !LeaveDocument()) return;
        CreateSceneAt(Utf8(path));
    }
    void CreateSceneAt(const std::string& path) {
        const auto result = model.CreateScene(path, fs::FileName(path));
        notice = result ? "Created scene. Use + Actor to add a cube, then Save." : result.Error().ToPretty();
        if (result) { selected = -1; sourceMode = false; ListProject(); Refresh(); }
        else StatusText();
    }
    void OpenDemo() {
        auto base = fs::ExecutableDirectory();
        for (int i = 0; i < 6; ++i) {
            const auto sample = fs::Join(base, "Samples/CubePlayground");
            if (fs::IsDirectory(sample)) {
                if (!LeaveDocument()) return;
                root = sample; ListProject(); Open(fs::Join(root, "scenes/main.scene.yaml")); return;
            }
            base = fs::ParentPath(base);
        }
        notice = "CubePlayground not found. Open project and choose Samples/CubePlayground."; StatusText();
    }
    void Tick() {
        if (!play.IsPlaying()) return;
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - lastTick).count(); lastTick = now;
        std::vector<std::string> keys;
        if (GetForegroundWindow() == window && GetFocus() == window && !sourceMode) {
            for (const auto& [key, token] : {std::pair<int, const char*>{'A', "key.a"}, {'D', "key.d"}, {'W', "key.w"}, {'S', "key.s"},
                {VK_CONTROL, "key.control"}, {VK_SPACE, "key.space"}, {'Q', "key.q"}, {'E', "key.e"}, {'F', "key.f"}, {'R', "key.r"}})
                if (GetAsyncKeyState(key) & 0x8000) keys.emplace_back(token);
        }
        const auto result = play.Tick(dt, keys);
        if (!result) { notice = result.Error().ToPretty(); play.Stop(); KillTimer(window, 2); Refresh(); return; }
        if (!inspectorDirty && GetFocus() == window) Inspector();
        InvalidateRect(window, &scene, FALSE);
    }
    void Command(int id) {
        if (id == Stop) { play.Stop(); KillTimer(window, 2); inspectorDirty = false; notice = "Stopped. Original scene restored."; Refresh(); return; }
        if (play.IsPlaying() && id != Apply && id != Validate && id != Frame && id != Scene) return;
        if (id == NewScene) { CreateScene(); return; }
        if (id == Demo) { OpenDemo(); return; }
        if (id == Reload) { if (LeaveDocument()) { ListProject(); Open(model.Path()); } return; }
        if (id == OpenProject) { ChooseProject(); return; }
        if (id == OpenFile) { ChooseFile(); return; }
        if (id == Save) { SaveFile(); return; }
        if (id == Frame) { viewport = {}; InvalidateRect(window, &scene, FALSE); return; }
        if (id == Undo && inspectorDirty) { Inspector(); notice = "Reverted pending inspector edits."; StatusText(); return; }
        if (!FlushEdits()) return;
        if (id == Play) {
            const auto result = play.Start(model, fs::Join(root, "input/default.input.yaml"));
            bool platformer = false;
            for (const auto& actor : model.Document().root["actors"].Items())
                platformer = platformer || actor["components"].Has("character2d");
            notice = result ? (platformer ? "Platformer: click Scene, A/D move, Space jumps when grounded. Stop to edit or restart."
                : "Play preview: click Scene, WASD move, Space/Ctrl height, Q/E rotate, F/R scale. Stop restores the document.") : result.Error().ToPretty();
            if (result) { sourceMode = false; lastTick = std::chrono::steady_clock::now(); SetTimer(window, 2, 16, nullptr); SetFocus(window); }
            Refresh(); return;
        }
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
        const auto geometry = workspace.Compute(width, height);
        const int bottom = geometry.bottom, right = geometry.right;
        Label(buffer, 16, geometry.projectTop - 23, L"PROJECT / DOCUMENTS");
        Label(buffer, geometry.editor + 16, 116, L"TERMINAL / PowerShell");
        Label(buffer, right + 16, 116, L"INSPECTOR"); Label(buffer, right + 16, 187, L"Name");
        for (int group = 0; group < 3; ++group) {
            Label(buffer, right + 16, 249 + group * 79, group == 0 ? L"Position" : group == 1 ? L"Rotation / degrees" : L"Scale");
            for (int axis = 0; axis < 3; ++axis) Label(buffer, right + 16 + axis * ((geometry.editor - right - 32) / 3), 271 + group * 79, axis == 0 ? L"X" : axis == 1 ? L"Y" : L"Z");
        }
        Label(buffer, right + 16, 585, play.IsPlaying() ? L"PLAY / temporary transforms" : L"Components / nested actors:");
        Label(buffer, right + 16, 607, play.IsPlaying() ? L"Stop restores saved edits." : L"edit in the Document tab.");
        Label(buffer, 16, bottom + 10, L"CONSOLE / VALIDATION");
        Label(buffer, 16, height - 22, L"Ctrl+S Save    Ctrl+Z / Ctrl+Y Undo / Redo    F5 Validate");
        if (!sourceMode) {
            viewport.Paint(buffer, scene, model.IsValid() ? Actors() : std::vector<Actor>{}, selected);
            Label(buffer, scene.left + 14, scene.top + 12, play.IsPlaying() ? L"PLAY PREVIEW / cubes" : L"SCENE / cubes");
            Label(buffer, scene.left + 14, scene.bottom - 28, play.IsPlaying() ? L"WASD move | Q/E turn | F/R scale" : L"Wheel: zoom | Middle-drag: pan");
        }
        HPEN separator = CreatePen(PS_SOLID, 2, RGB(66, 82, 97));
        const auto previousPen = SelectObject(buffer, separator);
        for (int divider : {geometry.left, geometry.right, geometry.editor}) { MoveToEx(buffer, divider, 102, nullptr); LineTo(buffer, divider, divider == geometry.editor ? height - 26 : bottom); }
        MoveToEx(buffer, 8, bottom, nullptr); LineTo(buffer, geometry.editor - 8, bottom);
        MoveToEx(buffer, 8, geometry.projectTop - 30, nullptr); LineTo(buffer, geometry.left - 8, geometry.projectTop - 30);
        SelectObject(buffer, previousPen); DeleteObject(separator);
        BitBlt(dc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, oldBitmap); DeleteObject(bitmap); DeleteDC(buffer); EndPaint(window, &paint);
    }
    void SmokeTest() {
        if (terminal.Running() && terminal.ScreenText().find("ALICE_EDITOR_READY") == std::string::npos && ++smokeWaits < 12) return;
        KillTimer(window, 1);
        bool ok = true;
        auto checks = doc::Value::MakeMap();
        auto check = [&](const char* key, bool passed) { checks.Set(key, doc::Value{passed}); ok = ok && passed; };
        check("window_and_scene", IsWindowVisible(window) && IsWindowVisible(hierarchy) && IsWindowVisible(name) && model.IsValid() && !model.Actors().empty());
        check("new_scene_button", IsWindowVisible(GetDlgItem(window, NewScene)) && IsWindowEnabled(GetDlgItem(window, NewScene)));
        check("real_terminal", IsWindowVisible(terminal.Handle()) && terminal.Running() &&
            terminal.ScreenText().find("ALICE_EDITOR_READY") != std::string::npos);
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
            SetWindowTextW(fields[0], L"8.75");
            CreateSceneAt(model.Path());
            check("failed_new_scene_preserves_inspector", inspectorDirty && Read(fields[0]) == L"8.75" && model.Text() == before);
            Command(Undo);
            Command(Source);
            SendMessageW(source, EM_SETSEL, 0, 0);
            SendMessageW(source, WM_CHAR, '#', 0); SendMessageW(source, WM_CHAR, '\r', 0);
            const auto pendingSource = Read(source);
            const bool sourcePending = sourceDirty && pendingSource != Lines(before) && model.Text() == before;
            CreateSceneAt(model.Path());
            check("failed_new_scene_preserves_source", sourcePending && sourceDirty && Read(source) == pendingSource && model.Text() == before);
            Command(Undo); Command(Scene);
            const auto geometry = workspace.Compute(width, height);
            SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(geometry.editor, 300));
            SendMessageW(window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(geometry.editor - 40, 300));
            SendMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(geometry.editor - 40, 300));
            check("splitter_drag", workspace.Compute(width, height).editor == geometry.editor - 40);
            if (root.find("CubePlayground") != std::string::npos) {
                Command(Play); check("play_started", play.IsPlaying());
                if (play.IsPlaying()) {
                    const auto initial = play.Actors();
                    const auto tick = play.Tick(.1, {"key.d", "key.e", "key.r"});
                    const auto moved = play.Actors();
                    check("play_input_transform", tick.IsOk() && moved[0].position[0] > initial[0].position[0] && moved[0].rotation[1] > initial[0].rotation[1] && moved[0].scale[0] > initial[0].scale[0]);
                    selected = 0; Inspector(); SetWindowTextW(fields[0], L"3.5"); Command(Apply);
                    check("play_inspector", play.Actors()[0].position[0] == 3.5 && model.Text() == before);
                    SetFocus(terminal.Handle()); check("terminal_focus", TerminalFocus());
                    Command(Stop); check("stop_restores_document", !play.IsPlaying() && model.Text() == before && !model.IsDirty());
                }
            }
            if (root.find("PlatformerDemo") != std::string::npos) {
                Command(Play); check("physics_started", play.IsPlaying());
                if (play.IsPlaying()) {
                    const auto initialY = play.Actors()[0].position[1];
                    bool stepped = true;
                    for (int i=0; i<120; ++i) stepped = play.Tick(1.0/60, {}).IsOk() && stepped;
                    const auto landedY = play.Actors()[0].position[1];
                    check("physics_landed", stepped && landedY < initialY && std::abs(landedY - 0.5) < 0.001);
                    const auto jump = play.Tick(1.0/60, {"key.space"});
                    check("physics_jumped", jump.IsOk() && play.Actors()[0].position[1] > landedY);
                    const auto x = play.Actors()[0].position[0];
                    const auto move = play.Tick(1.0/60, {"key.d"});
                    check("physics_moved", move.IsOk() && play.Actors()[0].position[0] > x);
                    Command(Stop); check("physics_stop_restores", !play.IsPlaying() && model.Text() == before && !model.IsDirty());
                }
            }
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
    case WM_GETMINMAXINFO: reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = {1120, 820}; return 0;
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
            if (app->play.IsPlaying()) return 0;
            const auto index = static_cast<usize>(SendMessageW(app->project, LB_GETCURSEL, 0, 0));
            if (index < app->files.size() && app->LeaveDocument()) app->Open(app->files[index]);
            return 0;
        }
        if (id >= OpenProject && id <= Demo) { app->Command(id); return 0; }
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
        app->split = app->workspace.Hit(point.x, point.y, app->width, app->height);
        if (app->split != Split::None) { SetCapture(window); return 0; }
        if (!app->sourceMode && PtInRect(&app->scene, point) && app->FlushEdits()) {
            const int hit = app->viewport.Hit(app->scene, point, app->Actors());
            if (hit >= 0) app->selected = hit;
            SetFocus(window); app->Refresh();
        }
        return 0;
    }
    case WM_SETCURSOR: {
        POINT point{}; GetCursorPos(&point); ScreenToClient(window, &point);
        const auto hit = app->workspace.Hit(point.x, point.y, app->width, app->height);
        if (LOWORD(lParam) == HTCLIENT && hit != Split::None) {
            SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(hit == Split::Console || hit == Split::Project ? 32645 : 32644))); return TRUE;
        }
        break;
    }
    case WM_LBUTTONUP: if (app->split != Split::None) { app->split = Split::None; ReleaseCapture(); } return 0;
    case WM_CAPTURECHANGED: app->split = Split::None; app->dragging = false; return 0;
    case WM_MBUTTONDOWN: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (!app->sourceMode && PtInRect(&app->scene, point)) { app->dragging = true; app->dragStart = point; SetCapture(window); }
        return 0;
    }
    case WM_MBUTTONUP: app->dragging = false; ReleaseCapture(); return 0;
    case WM_MOUSEMOVE:
        if (app->split != Split::None) {
            app->workspace.Drag(app->split, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), app->width, app->height); app->Layout(); return 0;
        }
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
    case WM_TIMER: if (wParam == 1 && app->smoke) app->SmokeTest(); else if (wParam == 2) app->Tick(); return 0;
    case WM_CLOSE: if (app->LeaveDocument()) DestroyWindow(window); return 0;
    case WM_DESTROY: app->terminal.Stop(); PostQuitMessage(app->exitCode); return 0;
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
            const auto sample = fs::Join(base, "Samples/CubePlayground");
            if (fs::IsDirectory(sample)) { app.root = sample; break; }
            base = fs::ParentPath(base);
        }
        if (app.root.empty()) app.root = fs::CurrentDirectory();
    }
    WNDCLASSW cls{}; cls.hInstance = instance; cls.lpfnWndProc = WindowProc;
    cls.lpszClassName = L"AliceSingularityEditor"; cls.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    cls.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512)); RegisterClassW(&cls);
    HWND window = CreateWindowExW(WS_EX_APPWINDOW, cls.lpszClassName, L"AliceEngine Singularity", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1680, 1000, nullptr, nullptr, instance, &app);
    if (!window) { if (SUCCEEDED(com)) CoUninitialize(); return 1; }
    ShowWindow(window, show); UpdateWindow(window);
    const ACCEL shortcuts[] = {{FVIRTKEY | FCONTROL, 'S', Save}, {FVIRTKEY | FCONTROL, 'O', OpenFile},
        {FVIRTKEY | FCONTROL, 'Z', Undo}, {FVIRTKEY | FCONTROL, 'Y', Redo}, {FVIRTKEY, VK_F5, Validate}};
    HACCEL accelerator = CreateAcceleratorTableW(const_cast<ACCEL*>(shortcuts), static_cast<int>(std::size(shortcuts)));
    MSG message{}; int result = 0;
    while ((result = static_cast<int>(GetMessageW(&message, nullptr, 0, 0))) > 0) {
        if (app.TerminalFocus() || (!TranslateAcceleratorW(window, accelerator, &message) && !IsDialogMessageW(window, &message))) {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
    }
    DestroyAcceleratorTable(accelerator); if (SUCCEEDED(com)) CoUninitialize();
    return result < 0 ? 1 : static_cast<int>(message.wParam);
}
} // namespace alice::editor
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) { return alice::editor::Run(instance, show); }
