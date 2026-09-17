// SPDX-License-Identifier: MIT
// ConPTY keeps shells and interactive CLIs attached to a genuine console instead of redirected standard input.
#include "AliceEditor/TerminalSession.h"
#ifdef _WIN32
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <cwctype>
#include <vector>
namespace alice::editor {
namespace {
std::wstring Wide(std::string_view text) {
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (length != 0) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), length);
    return result;
}
void Close(HANDLE& handle) { if (handle) { CloseHandle(handle); handle = nullptr; } }
std::vector<wchar_t> Environment(const std::wstring& engineBin) {
    std::vector<std::wstring> entries;
    bool pathFound = false;
    if (auto block = GetEnvironmentStringsW()) {
        for (const wchar_t* entry = block; *entry; entry += wcslen(entry) + 1) {
            std::wstring value(entry);
            if (_wcsnicmp(entry, L"PATH=", 5) == 0) { value = L"PATH=" + engineBin + L";" + (entry + 5); pathFound = true; }
            if (_wcsnicmp(entry, L"PSModulePath=", 13) == 0) {
                // A GUI launched by PowerShell 7 inherits Core-only modules; Windows PowerShell needs its Desktop modules.
                // Preserve custom paths while removing the three standard Core locations, without changing execution policy.
                const std::wstring paths(entry + 13);
                value = L"PSModulePath=";
                for (std::size_t start = 0; start < paths.size();) {
                    const auto end = paths.find(L';', start);
                    const auto path = paths.substr(start, end == std::wstring::npos ? end : end - start);
                    auto lower = path;
                    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
                    if (lower.find(L"\\powershell\\modules") == std::wstring::npos && lower.find(L"\\powershell\\7") == std::wstring::npos &&
                        lower.find(L"\\windowsapps\\microsoft.powershell_") == std::wstring::npos) {
                        if (value.back() != L'=') value += L';';
                        value += path;
                    }
                    if (end == std::wstring::npos) break;
                    start = end + 1;
                }
            }
            if (_wcsnicmp(entry, L"TERM=", 5) == 0 || _wcsnicmp(entry, L"COLORTERM=", 10) == 0) continue;
            entries.push_back(std::move(value));
        }
        FreeEnvironmentStringsW(block);
    }
    if (!pathFound) entries.push_back(L"PATH=" + engineBin);
    entries.push_back(L"TERM=xterm-256color"); entries.push_back(L"COLORTERM=truecolor");
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
    std::vector<wchar_t> result;
    for (const auto& value : entries) { result.insert(result.end(), value.begin(), value.end()); result.push_back(L'\0'); }
    result.push_back(L'\0');
    return result;
}
}
struct TerminalSession::Impl {
    using CreateFn = HRESULT (WINAPI*)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
    using ResizeFn = HRESULT (WINAPI*)(HPCON, COORD);
    using CloseFn = void (WINAPI*)(HPCON);
    CreateFn createConsole = nullptr;
    ResizeFn resizeConsole = nullptr;
    CloseFn closeConsole = nullptr;
    HPCON console = nullptr;
    HANDLE input = nullptr, output = nullptr, process = nullptr, job = nullptr;
    HANDLE reader = nullptr, writer = nullptr, wake = nullptr;
    mutable std::mutex screenMutex;
    std::mutex inputMutex;
    TerminalScreen screen;
    std::string pendingInput, directory;
    std::atomic<bool> stopping{false};
    Diagnostic Failure(const char* code, const char* message, const char* hint, DWORD error = GetLastError()) const {
        auto diagnostic = MakeError(code, std::string(message) + " (Windows " + std::to_string(error) + ")", hint);
        diagnostic.file = directory;
        diagnostic.path = "terminal";
        diagnostic.mark = {1, 1, 0};
        return diagnostic;
    }
    Status Queue(std::string_view bytes) {
        std::lock_guard lock(inputMutex);
        if (stopping || !input) return Failure("editor.terminal.closed", "terminal session is closed", "Reopen the editor to start a new terminal session", ERROR_BROKEN_PIPE);
        // A stalled console must not consume unlimited editor memory or block the UI on a large paste.
        if (bytes.size() > 1024 * 1024 - pendingInput.size())
            return Failure("editor.terminal.input_full", "terminal input queue is full", "Wait for the running command to read input, then paste a smaller amount", ERROR_NOT_ENOUGH_QUOTA);
        pendingInput.append(bytes); SetEvent(wake); return Status::Ok();
    }
    static DWORD WINAPI ReadPump(void* context) {
        auto& self = *static_cast<Impl*>(context);
        char bytes[8192]; DWORD count = 0;
        while (ReadFile(self.output, bytes, sizeof(bytes), &count, nullptr) && count != 0) {
            std::string replies;
            {
                std::lock_guard lock(self.screenMutex);
                self.screen.Feed(std::string_view(bytes, count));
                replies = self.screen.TakeReplies();
            }
            if (!replies.empty()) self.Queue(replies);
        }
        return 0;
    }
    static DWORD WINAPI WritePump(void* context) {
        auto& self = *static_cast<Impl*>(context);
        while (!self.stopping) {
            WaitForSingleObject(self.wake, INFINITE);
            std::string bytes;
            { std::lock_guard lock(self.inputMutex); bytes.swap(self.pendingInput); }
            std::size_t offset = 0;
            while (!self.stopping && offset < bytes.size()) {
                DWORD count = 0;
                if (!WriteFile(self.input, bytes.data() + offset, static_cast<DWORD>(bytes.size() - offset), &count, nullptr) || count == 0) return 0;
                offset += count;
            }
        }
        return 0;
    }
    void Stop() {
        stopping = true;
        if (wake) SetEvent(wake);
        if (job) TerminateJobObject(job, 0);
        // ClosePseudoConsole may emit a final frame. Keep ReadPump alive until this call returns.
        if (console) { closeConsole(console); console = nullptr; }
        if (writer) { CancelSynchronousIo(writer); WaitForSingleObject(writer, INFINITE); Close(writer); }
        Close(input);
        if (reader) { CancelSynchronousIo(reader); WaitForSingleObject(reader, INFINITE); Close(reader); }
        Close(output); Close(process); Close(job); Close(wake);
        pendingInput.clear();
    }
};
TerminalSession::TerminalSession() : m_impl(std::make_unique<Impl>()) {}
TerminalSession::~TerminalSession() { Stop(); }
Status TerminalSession::Start(const std::string& projectDirectory, const std::string& engineBinDirectory, int columns, int rows) {
    Stop(); auto& state = *m_impl; state.directory = projectDirectory; state.stopping = false;
    const auto cwd = Wide(projectDirectory);
    const DWORD attributes = GetFileAttributesW(cwd.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
        return state.Failure("editor.terminal.directory", "terminal project directory does not exist", "Open a project in an existing directory before starting the terminal", ERROR_PATH_NOT_FOUND);
    const auto kernel = GetModuleHandleW(L"kernel32.dll");
    // Resolve dynamically so unsupported Windows versions can still show a useful editor diagnostic.
    const auto load = [&](const char* name, auto& function) {
        const auto address = GetProcAddress(kernel, name);
        static_assert(sizeof(function) == sizeof(address));
        memcpy(&function, &address, sizeof(function));
    };
    load("CreatePseudoConsole", state.createConsole); load("ResizePseudoConsole", state.resizeConsole); load("ClosePseudoConsole", state.closeConsole);
    if (!state.createConsole || !state.resizeConsole || !state.closeConsole)
        return state.Failure("editor.terminal.unsupported", "Windows does not provide ConPTY", "Use Windows 10 version 1809 or newer to run the embedded terminal", ERROR_NOT_SUPPORTED);
    HANDLE consoleInput = nullptr, consoleOutput = nullptr;
    const auto fail = [&](Diagnostic diagnostic) -> Status { Close(consoleInput); Close(consoleOutput); state.Stop(); return diagnostic; };
    if (!CreatePipe(&consoleInput, &state.input, nullptr, 0) || !CreatePipe(&state.output, &consoleOutput, nullptr, 0))
        return fail(state.Failure("editor.terminal.pipe", "cannot create terminal communication pipes", "Close unused processes and reopen the terminal"));
    columns = std::clamp(columns, 2, 512); rows = std::clamp(rows, 1, 256);
    state.screen = TerminalScreen(columns, rows);
    const HRESULT created = state.createConsole(COORD{static_cast<SHORT>(columns), static_cast<SHORT>(rows)}, consoleInput, consoleOutput, 0, &state.console);
    if (FAILED(created)) return fail(state.Failure("editor.terminal.create", "cannot create a Windows pseudoconsole", "Use Windows 10 version 1809 or newer and reopen the editor", static_cast<DWORD>(created)));
    Close(consoleInput); Close(consoleOutput);
    state.wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!state.wake) return fail(state.Failure("editor.terminal.thread", "cannot create terminal input event", "Close unused processes and reopen the editor"));
    state.reader = CreateThread(nullptr, 0, Impl::ReadPump, &state, 0, nullptr);
    state.writer = CreateThread(nullptr, 0, Impl::WritePump, &state, 0, nullptr);
    if (!state.reader || !state.writer) {
        // If no pump exists, closing its pipe prevents ConPTY from waiting for a frame consumer.
        if (!state.reader) Close(state.output);
        return fail(state.Failure("editor.terminal.thread", "cannot start terminal communication threads", "Close unused processes and reopen the editor"));
    }
    SIZE_T attributeBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
    std::vector<unsigned char> attributeStorage(attributeBytes);
    auto* attributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
    if (!InitializeProcThreadAttributeList(attributeList, 1, 0, &attributeBytes))
        return fail(state.Failure("editor.terminal.start", "cannot initialize shell startup attributes", "Close unused processes and reopen the editor"));
    if (!UpdateProcThreadAttribute(attributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, state.console, sizeof(HPCON), nullptr, nullptr)) {
        auto diagnostic = state.Failure("editor.terminal.start", "cannot attach the shell to the pseudoconsole", "Use a Windows version with ConPTY support and reopen the editor");
        DeleteProcThreadAttributeList(attributeList); return fail(diagnostic);
    }
    state.job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!state.job || !SetInformationJobObject(state.job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        auto diagnostic = state.Failure("editor.terminal.job", "cannot create the terminal process group", "Allow the editor to create a Windows job object and reopen it");
        DeleteProcThreadAttributeList(attributeList); return fail(diagnostic);
    }
    wchar_t systemDirectory[MAX_PATH]{};
    GetSystemDirectoryW(systemDirectory, MAX_PATH);
    const std::wstring executable = std::wstring(systemDirectory) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    std::wstring command = L"\"" + executable + L"\" -NoLogo -NoProfile";
    auto environment = Environment(Wide(engineBinDirectory));
    STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup); startup.lpAttributeList = attributeList;
    // A parent launched with redirected stdio otherwise duplicates those handles even when inheritance is FALSE.
    // Null explicit handles let ConPTY install the console's handles (Microsoft Terminal discussion #15814).
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    PROCESS_INFORMATION process{};
    const BOOL launched = CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED,
        environment.data(), cwd.c_str(), &startup.StartupInfo, &process);
    const DWORD launchError = GetLastError();
    DeleteProcThreadAttributeList(attributeList);
    if (!launched) return fail(state.Failure("editor.terminal.start", "cannot start PowerShell", "Verify Windows PowerShell is installed and the project directory is accessible", launchError));
    state.process = process.hProcess;
    if (!AssignProcessToJobObject(state.job, process.hProcess)) {
        auto diagnostic = state.Failure("editor.terminal.job", "cannot attach PowerShell to the editor process group", "Run the editor outside a restrictive parent job and reopen the terminal");
        TerminateProcess(process.hProcess, 1); CloseHandle(process.hThread); return fail(diagnostic);
    }
    if (ResumeThread(process.hThread) == static_cast<DWORD>(-1)) {
        auto diagnostic = state.Failure("editor.terminal.start", "cannot resume PowerShell", "Close unused processes and reopen the terminal");
        CloseHandle(process.hThread); return fail(diagnostic);
    }
    CloseHandle(process.hThread);
    return Status::Ok();
}
Status TerminalSession::Send(std::string_view utf8) {
    if (!Running()) return m_impl->Failure("editor.terminal.closed", "terminal process has exited", "Reopen the editor to start a new terminal session", ERROR_BROKEN_PIPE);
    return m_impl->Queue(utf8);
}
Status TerminalSession::Resize(int columns, int rows) {
    auto& state = *m_impl;
    if (!state.console) return state.Failure("editor.terminal.closed", "terminal session is closed", "Reopen the editor to start a new terminal session", ERROR_BROKEN_PIPE);
    columns = std::clamp(columns, 2, 512); rows = std::clamp(rows, 1, 256);
    // Change the screen first; the output pump can then apply the redraw emitted by ConPTY at its new size.
    { std::lock_guard lock(state.screenMutex); state.screen.Resize(columns, rows); }
    const auto resized = state.resizeConsole(state.console, COORD{static_cast<SHORT>(columns), static_cast<SHORT>(rows)});
    if (FAILED(resized)) return state.Failure("editor.terminal.resize", "cannot resize the terminal", "Reopen the terminal after the current command exits", static_cast<DWORD>(resized));
    return Status::Ok();
}
TerminalScreen TerminalSession::Snapshot() const { std::lock_guard lock(m_impl->screenMutex); return m_impl->screen; }
bool TerminalSession::Running() const { return !m_impl->stopping && m_impl->process && WaitForSingleObject(m_impl->process, 0) == WAIT_TIMEOUT; }
void TerminalSession::Stop() { m_impl->Stop(); }
}
#endif
