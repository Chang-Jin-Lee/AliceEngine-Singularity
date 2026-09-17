// SPDX-License-Identifier: MIT
// Keep an interactive shell inside the editor without coupling its lifetime to scene documents.
#pragma once
#include "Foundation/Result.h"
#include <memory>
#include <string>
#include <string_view>
#ifdef _WIN32
#include <windows.h>
namespace alice::editor {
class TerminalPane {
public:
    TerminalPane();
    ~TerminalPane();
    TerminalPane(const TerminalPane&) = delete;
    TerminalPane& operator=(const TerminalPane&) = delete;
    Status Create(HWND parent, HINSTANCE instance, const std::string& projectDirectory,
                  const std::string& engineBinDirectory);
    HWND Handle() const;
    void Resize(int x, int y, int width, int height);
    Status Send(std::string_view utf8);
    std::string ScreenText() const;
    bool Running() const;
    void Stop();
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
#endif
