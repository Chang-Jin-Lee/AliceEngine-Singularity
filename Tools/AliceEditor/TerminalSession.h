// SPDX-License-Identifier: MIT
// Own the pseudoconsole and its independent input/output pumps, so the UI never writes a blocking pipe.
#pragma once
#include "Foundation/Result.h"
#include "AliceEditor/TerminalScreen.h"
#include <memory>
#include <string>
#include <string_view>
#ifdef _WIN32
namespace alice::editor {
class TerminalSession {
public:
    TerminalSession();
    ~TerminalSession();
    TerminalSession(const TerminalSession&) = delete;
    TerminalSession& operator=(const TerminalSession&) = delete;
    Status Start(const std::string& projectDirectory, const std::string& engineBinDirectory, int columns, int rows);
    Status Send(std::string_view utf8);
    Status Resize(int columns, int rows);
    TerminalScreen Snapshot() const;
    bool Running() const;
    void Stop();
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
#endif
