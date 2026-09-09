// SPDX-License-Identifier: MIT
// The extraction adapter alone joins ECS data to the renderer's independent packet format.
#pragma once

#include "Runtime/RenderView.h"
#include "Runtime/World.h"

namespace alice::runtime {

// Current committed transforms; no interpolation, asset loading, or GPU calls.
// Missing optional render components produce empty lists; invalid extraction reports a diagnostic.
Result<RenderView> ExtractRenderView(const WorldReadView& world);

} // namespace alice::runtime
