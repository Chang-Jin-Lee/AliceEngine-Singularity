// SPDX-License-Identifier: MIT
// Preview state belongs to a disposable ECS world, never to the editable document.
#pragma once
#include "AliceEditor/DocumentModel.h"
#include <memory>

namespace alice::editor {
class PlaySession {
public:
    PlaySession();
    ~PlaySession();
    PlaySession(const PlaySession&) = delete;
    PlaySession& operator=(const PlaySession&) = delete;
    Status Start(const DocumentModel& document, const std::string& inputPath);
    void Stop();
    bool IsPlaying() const;
    std::vector<Actor> Actors() const;
    Status EditActor(usize index, const Actor& actor);
    // The window supplies empty heldKeys whenever its viewport does not have focus.
    Status Tick(double dt, const std::vector<std::string>& heldKeys);
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
} // namespace alice::editor
