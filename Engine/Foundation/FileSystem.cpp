// SPDX-License-Identifier: MIT
#include "FileSystem.h"

#include "StringUtil.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <system_error>

#if ALICE_PLATFORM_WINDOWS
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace alice::fs {
namespace {

namespace stdfs = std::filesystem;

/// UTF-8 문자열 → std::filesystem::path.
/// Windows 에서 std::filesystem::path(std::string) 은 **ANSI(CP949)** 로 해석한다.
/// 그래서 UTF-8 을 명시적으로 UTF-16 으로 올려준다. 이걸 빼면 한글 경로가 깨진다.
stdfs::path ToPath(std::string_view utf8) {
#if ALICE_PLATFORM_WINDOWS
    if (utf8.empty()) return {};
    const int wide = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                           static_cast<int>(utf8.size()), nullptr, 0);
    if (wide <= 0) return stdfs::path(std::string(utf8));
    std::wstring w(static_cast<usize>(wide), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), w.data(), wide);
    return stdfs::path(w);
#else
    return stdfs::path(std::string(utf8));
#endif
}

/// std::filesystem::path → UTF-8 문자열. 구분자는 '/' 로 통일한다.
std::string FromPath(const stdfs::path& p) {
#if ALICE_PLATFORM_WINDOWS
    const std::wstring w = p.wstring();
    if (w.empty()) return {};
    const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                                            nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<usize>(bytes), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                          out.data(), bytes, nullptr, nullptr);
    for (char& c : out) if (c == '\\') c = '/';
    return out;
#else
    std::string out = p.string();
    for (char& c : out) if (c == '\\') c = '/';
    return out;
#endif
}

Diagnostic IoError(const std::string& code, const std::string& path, const std::error_code& ec) {
    Diagnostic d = MakeError("fs." + code, Fmt("{}: {}", path, ec.message()));
    d.file = path;
    return d;
}

std::FILE* OpenFile(const std::string& path, const char* mode) {
#if ALICE_PLATFORM_WINDOWS
    const stdfs::path p = ToPath(path);
    const std::wstring wmode(mode, mode + std::char_traits<char>::length(mode));
    std::FILE* f = nullptr;
    _wfopen_s(&f, p.c_str(), wmode.c_str());
    return f;
#else
    return std::fopen(path.c_str(), mode);
#endif
}

} // namespace

bool Exists(const std::string& path) {
    std::error_code ec;
    return stdfs::exists(ToPath(path), ec);
}

bool IsDirectory(const std::string& path) {
    std::error_code ec;
    return stdfs::is_directory(ToPath(path), ec);
}

bool IsFile(const std::string& path) {
    std::error_code ec;
    return stdfs::is_regular_file(ToPath(path), ec);
}

Result<std::string> ReadTextFile(const std::string& path) {
    std::FILE* f = OpenFile(path, "rb");
    if (!f) {
        Diagnostic d = MakeError("fs.read_failed", Fmt("파일을 열 수 없다: {}", path),
                                 "경로와 권한을 확인하라. 상대경로면 작업 디렉터리 기준이다");
        d.file = path;
        return d;
    }
    ALICE_DEFER(std::fclose(f));

    std::string out;
    char buffer[16 * 1024];
    while (true) {
        const usize n = std::fread(buffer, 1, sizeof(buffer), f);
        if (n == 0) break;
        out.append(buffer, n);
    }

    // UTF-8 BOM 은 파서를 헷갈리게 하므로 여기서 벗긴다.
    if (out.size() >= 3 &&
        static_cast<u8>(out[0]) == 0xEF &&
        static_cast<u8>(out[1]) == 0xBB &&
        static_cast<u8>(out[2]) == 0xBF) {
        out.erase(0, 3);
    }
    // CRLF → LF. 파서의 열 계산과 스니펫이 플랫폼에 따라 달라지면 안 된다.
    if (out.find('\r') != std::string::npos) out = Replace(out, "\r\n", "\n");
    return out;
}

Result<std::vector<u8>> ReadBinaryFile(const std::string& path) {
    std::FILE* f = OpenFile(path, "rb");
    if (!f) {
        Diagnostic d = MakeError("fs.read_failed", Fmt("파일을 열 수 없다: {}", path));
        d.file = path;
        return d;
    }
    ALICE_DEFER(std::fclose(f));

    std::vector<u8> out;
    u8 buffer[16 * 1024];
    while (true) {
        const usize n = std::fread(buffer, 1, sizeof(buffer), f);
        if (n == 0) break;
        out.insert(out.end(), buffer, buffer + n);
    }
    return out;
}

Status WriteTextFile(const std::string& path, std::string_view contents) {
    const std::string parent = ParentPath(path);
    if (!parent.empty() && !Exists(parent)) {
        Status s = CreateDirectories(parent);
        if (s.IsErr()) return s;
    }

    std::FILE* f = OpenFile(path, "wb");
    if (!f) {
        Diagnostic d = MakeError("fs.write_failed", Fmt("파일을 쓸 수 없다: {}", path));
        d.file = path;
        return d;
    }
    ALICE_DEFER(std::fclose(f));
    if (!contents.empty()) std::fwrite(contents.data(), 1, contents.size(), f);
    return Status::Ok();
}

Status WriteBinaryFile(const std::string& path, const std::vector<u8>& bytes) {
    const std::string parent = ParentPath(path);
    if (!parent.empty() && !Exists(parent)) {
        Status s = CreateDirectories(parent);
        if (s.IsErr()) return s;
    }

    std::FILE* f = OpenFile(path, "wb");
    if (!f) {
        Diagnostic d = MakeError("fs.write_failed", Fmt("파일을 쓸 수 없다: {}", path));
        d.file = path;
        return d;
    }
    ALICE_DEFER(std::fclose(f));
    if (!bytes.empty()) std::fwrite(bytes.data(), 1, bytes.size(), f);
    return Status::Ok();
}

Status CreateDirectories(const std::string& path) {
    std::error_code ec;
    stdfs::create_directories(ToPath(path), ec);
    if (ec && !IsDirectory(path)) return IoError("mkdir_failed", path, ec);
    return Status::Ok();
}

std::vector<std::string> ListFiles(const std::string& directory,
                                   std::string_view extension,
                                   bool recursive) {
    std::vector<std::string> out;
    std::error_code ec;
    const stdfs::path root = ToPath(directory);
    if (!stdfs::is_directory(root, ec)) return out;

    const std::string wanted = ToLower(extension);

    auto consider = [&](const stdfs::directory_entry& e) {
        if (!e.is_regular_file()) return;
        const std::string p = FromPath(e.path());
        if (!wanted.empty() && !EndsWith(ToLower(p), wanted)) return;
        out.push_back(p);
    };

    if (recursive) {
        for (auto it = stdfs::recursive_directory_iterator(root, ec);
             it != stdfs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) break;
            consider(*it);
        }
    } else {
        for (auto it = stdfs::directory_iterator(root, ec);
             it != stdfs::directory_iterator(); it.increment(ec)) {
            if (ec) break;
            consider(*it);
        }
    }

    std::sort(out.begin(), out.end());   // 결정적 순서. 빌드 재현성에 필요하다.
    return out;
}

std::string Extension(const std::string& path) {
    const std::string name = FileName(path);
    const usize dot = name.find_last_of('.');
    if (dot == std::string::npos) return {};
    return ToLower(name.substr(dot));
}

std::string FileName(const std::string& path) {
    const std::string n = Normalize(path);
    const usize slash = n.find_last_of('/');
    return slash == std::string::npos ? n : n.substr(slash + 1);
}

std::string FileStem(const std::string& path) {
    const std::string name = FileName(path);
    const usize dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

std::string ParentPath(const std::string& path) {
    const std::string n = Normalize(path);
    const usize slash = n.find_last_of('/');
    return slash == std::string::npos ? std::string{} : n.substr(0, slash);
}

std::string Join(std::string_view a, std::string_view b) {
    if (a.empty()) return std::string(b);
    if (b.empty()) return std::string(a);
    std::string out(a);
    if (out.back() != '/' && out.back() != '\\') out += '/';
    out.append(b.front() == '/' ? b.substr(1) : b);
    return Normalize(out);
}

std::string Normalize(std::string_view path) {
    std::string out(path);
    for (char& c : out) if (c == '\\') c = '/';
    // "//" 를 접는다. 단, UNC 접두사("//server/share")는 남긴다.
    const usize start = StartsWith(out, "//") ? 2 : 0;
    std::string collapsed = out.substr(0, start);
    for (usize i = start; i < out.size(); ++i) {
        if (out[i] == '/' && !collapsed.empty() && collapsed.back() == '/') continue;
        collapsed += out[i];
    }
    return collapsed;
}

std::string ExecutableDirectory() {
#if ALICE_PLATFORM_WINDOWS
    std::wstring buffer(1024, L'\0');
    const DWORD n = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(n);
    return ParentPath(FromPath(stdfs::path(buffer)));
#else
    std::error_code ec;
    const stdfs::path p = stdfs::read_symlink("/proc/self/exe", ec);
    if (!ec) return ParentPath(FromPath(p));
    return CurrentDirectory();
#endif
}

std::string CurrentDirectory() {
    std::error_code ec;
    return FromPath(stdfs::current_path(ec));
}

} // namespace alice::fs
