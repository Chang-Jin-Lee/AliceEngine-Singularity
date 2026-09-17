// SPDX-License-Identifier: MIT
// A rejected save must leave the original file recoverable and the editor dirty.
#include "AliceEditor/DocumentModel.h"
#include "Foundation/FileSystem.h"
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace alice::editor {
static Status AtomicWrite(const std::string& path, std::string_view text, std::string_view expected, bool create) {
    auto failure = [&](const char* code, const char* message, const char* hint) -> Status {
        auto d = MakeError(code, message, hint); d.file = path; d.path = "document"; d.mark = {1, 1, 0}; return d;
    };
    const auto before = fs::ReadTextFile(path);
    if (create && fs::Exists(path)) return failure("editor.create.exists", "A file already exists at this path",
        "Choose a new scene filename; existing documents are never overwritten by New scene");
    if (!create && (!before || before.Value() != expected)) return failure("editor.save.conflict", "The file changed outside this editor",
        "Copy your edits, reopen the file and reconcile the external changes before saving");
    const auto nativePath = [](std::string_view value) {
        return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(value.data()), value.size()));
    };
    const auto target = nativePath(path);
    const auto temporary = nativePath(path + ".alice-editor.tmp");
    bool written = false;
#ifdef _WIN32
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return failure("editor.save.temporary_failed", "Cannot create the temporary save file",
        "Check directory permissions and recover any existing .alice-editor.tmp file before retrying");
    usize offset = 0;
    while (offset < text.size()) {
        DWORD count = 0;
        const DWORD chunk = static_cast<DWORD>((std::min)(text.size() - offset, usize{1048576}));
        if (!WriteFile(file, text.data() + offset, chunk, &count, nullptr) || count == 0) break;
        offset += count;
    }
    written = offset == text.size() && FlushFileBuffers(file); CloseHandle(file);
#else
    const int file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) return failure("editor.save.temporary_failed", "Cannot create the temporary save file",
        "Check directory permissions and recover any existing .alice-editor.tmp file before retrying");
    usize offset = 0;
    while (offset < text.size()) {
        const auto count = write(file, text.data() + offset, text.size() - offset);
        if (count <= 0) break;
        offset += static_cast<usize>(count);
    }
    written = offset == text.size() && fsync(file) == 0; close(file);
#endif
    std::error_code ec;
    const auto latest = fs::ReadTextFile(path);
    if (!written || (!create && (!latest || latest.Value() != expected))) {
        std::filesystem::remove(temporary, ec);
        return failure("editor.save.failed", "Could not finish the save or the original changed during writing",
            "Check free disk space and permissions; reopen external changes before retrying");
    }
#ifdef _WIN32
    const bool replaced = create ? MoveFileExW(temporary.c_str(), target.c_str(), 0) != FALSE
                                 : ReplaceFileW(target.c_str(), temporary.c_str(), nullptr, 0, nullptr, nullptr) != FALSE;
    if (!replaced) ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
    if (create) {
        // Linking publishes the complete file without replacing a concurrently created destination.
        if (link(temporary.c_str(), target.c_str()) != 0) ec = std::error_code(errno, std::generic_category());
        if (!ec) std::filesystem::remove(temporary, ec);
    } else {
        const auto permissions = std::filesystem::status(target, ec).permissions();
        if (!ec) std::filesystem::permissions(temporary, permissions, ec);
        if (!ec) std::filesystem::rename(temporary, target, ec);
    }
#endif
    if (ec) {
        std::error_code cleanup; std::filesystem::remove(temporary, cleanup);
        return failure("editor.save.replace_failed", "Could not replace the original file",
            "Close programs locking this file and check write permissions before retrying");
    }
    return Status::Ok();
}
Status AtomicSave(const std::string& path, std::string_view text, std::string_view expected) {
    return AtomicWrite(path, text, expected, false);
}
Status AtomicCreate(const std::string& path, std::string_view text) {
    return AtomicWrite(path, text, {}, true);
}
} // namespace alice::editor
