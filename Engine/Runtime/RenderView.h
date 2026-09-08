// SPDX-License-Identifier: MIT
// Owning CPU packets let renderers consume a frame after its World and read lease are gone.
#pragma once

#include "Foundation/Core.h"

#include <array>
#include <string>
#include <vector>

namespace alice::runtime {

// Column-major, column vectors, metres, left-handed +Y up / +Z forward. GPU conversion is external.
using Matrix4 = std::array<f32, 16>;
inline constexpr Matrix4 kIdentityMatrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
using LinearColor = std::array<f32, 4>;

struct MeshInstance {
    Matrix4 worldFromLocal = kIdentityMatrix;
    std::string asset;                    // Project-relative asset path, not an RHI handle.
    std::vector<std::string> materials;   // Empty uses asset defaults; last override repeats.
    bool castShadow = true;
    bool receiveShadow = true;
    f32 lodBias = 1;
};

enum class Projection : u8 { Perspective, Orthographic };
struct CameraView {
    Matrix4 worldFromCamera = kIdentityMatrix;
    Projection projection = Projection::Perspective;
    f32 verticalFieldOfViewDegrees = 60;
    f32 orthographicHalfHeight = 5;
    f32 nearClip = 0.1f;
    f32 farClip = 1000;
    i32 priority = 0;
    bool hasClearColor = false;
    LinearColor clearColor{0, 0, 0, 1};
};

enum class LightType : u8 { Directional, Point, Spot };
struct LightInstance {
    Matrix4 worldFromLight = kIdentityMatrix;
    LightType type = LightType::Directional;
    LinearColor color{1, 1, 1, 1};
    f32 intensity = 1;                    // Directional: lux; point/spot: lumens.
    f32 range = 10;
    f32 spotAngleDegrees = 45;            // Full cone opening; shading uses half this angle.
    bool castShadow = false;
    f32 shadowBias = 0.005f;
};

struct RenderView {
    std::vector<MeshInstance> meshes;
    std::vector<CameraView> cameras;        // Highest priority first; ties retain entity order.
    std::vector<LightInstance> lights;
};

} // namespace alice::runtime
