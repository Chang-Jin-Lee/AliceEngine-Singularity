// SPDX-License-Identifier: MIT
#include "RHIBackend.h"

#include "Foundation/Log.h"
#include "Foundation/StringUtil.h"

#include <array>
#include <mutex>

namespace alice::rhi {
namespace {

constexpr const char* kChannel = "rhi";

struct FactoryTable {
    std::mutex mutex;
    std::array<DeviceFactory, static_cast<usize>(Backend::Count)> factories;
};

FactoryTable& Table() {
    static FactoryTable t;
    return t;
}

} // namespace

const char* ToString(Backend b) noexcept {
    switch (b) {
        case Backend::Null:   return "null";
        case Backend::D3D11:  return "d3d11";
        case Backend::D3D12:  return "d3d12";
        case Backend::Vulkan: return "vulkan";
        case Backend::Metal:  return "metal";
        case Backend::Count:  return "auto";
    }
    return "unknown";
}

bool ParseBackend(std::string_view name, Backend& out) noexcept {
    if (EqualsIgnoreCase(name, "null"))   { out = Backend::Null;   return true; }
    if (EqualsIgnoreCase(name, "d3d11"))  { out = Backend::D3D11;  return true; }
    if (EqualsIgnoreCase(name, "d3d12"))  { out = Backend::D3D12;  return true; }
    if (EqualsIgnoreCase(name, "vulkan")) { out = Backend::Vulkan; return true; }
    if (EqualsIgnoreCase(name, "metal"))  { out = Backend::Metal;  return true; }
    if (EqualsIgnoreCase(name, "auto"))   { out = Backend::Count;  return true; }
    return false;
}

void RegisterBackend(Backend backend, DeviceFactory factory) {
    if (backend >= Backend::Count) return;
    FactoryTable& t = Table();
    std::lock_guard<std::mutex> lock(t.mutex);
    t.factories[static_cast<usize>(backend)] = std::move(factory);
}

std::vector<Backend> AvailableBackends() {
    std::vector<Backend> out;
    FactoryTable& t = Table();
    std::lock_guard<std::mutex> lock(t.mutex);
    for (usize i = 0; i < t.factories.size(); ++i) {
        if (t.factories[i]) out.push_back(static_cast<Backend>(i));
    }
    return out;
}

bool IsBackendAvailable(Backend backend) noexcept {
    if (backend >= Backend::Count) return false;
    FactoryTable& t = Table();
    std::lock_guard<std::mutex> lock(t.mutex);
    return static_cast<bool>(t.factories[static_cast<usize>(backend)]);
}

std::vector<Backend> PreferredBackendOrder() {
    // Null 은 항상 마지막이다. 앞의 전부가 실패해도 엔진은 뜬다 —
    // 헤드리스로라도 떠야 콘텐츠 검증과 CI 가 돌아간다.
#if ALICE_PLATFORM_WINDOWS
    return {Backend::D3D12, Backend::D3D11, Backend::Vulkan, Backend::Null};
#elif ALICE_PLATFORM_APPLE
    return {Backend::Metal, Backend::Vulkan, Backend::Null};
#else
    return {Backend::Vulkan, Backend::Null};
#endif
}

Result<std::unique_ptr<IDevice>> CreateDevice(const DeviceCreateInfo& info) {
    // 명시적 선택.
    if (info.backend != Backend::Count) {
        DeviceFactory factory;
        {
            FactoryTable& t = Table();
            std::lock_guard<std::mutex> lock(t.mutex);
            factory = t.factories[static_cast<usize>(info.backend)];
        }
        if (!factory) {
            std::vector<std::string> names;
            for (const Backend b : AvailableBackends()) names.emplace_back(ToString(b));

            ALICE_LOG_ERROR(kChannel, "rhi.backend.unavailable")
                .Msg("요청한 백엔드가 이 빌드에 없다")
                .F("backend", ToString(info.backend))
                .F("available", Join(names, ", "));

            return MakeError("rhi.backend_unavailable",
                             Fmt("백엔드 '{}' 가 이 빌드에 포함되지 않았다", ToString(info.backend)),
                             Fmt("빌드에 있는 백엔드: {}. CMake 옵션 ALICE_RHI_* 를 확인하라",
                                    Join(names, ", ")));
        }

        Result<std::unique_ptr<IDevice>> device = factory(info);
        if (device.IsErr()) {
            ALICE_LOG_ERROR(kChannel, "rhi.backend.create_failed")
                .Msg("백엔드 생성에 실패했다")
                .F("backend", ToString(info.backend))
                .F("reason", device.Error().message);
            return device;
        }

        ALICE_LOG_INFO(kChannel, "rhi.backend.selected")
            .Msg("그래픽 백엔드를 선택했다")
            .F("backend", ToString(info.backend))
            .F("device", device.Value()->GetCapabilities().deviceName)
            .F("explicit", true);
        return device;
    }

    // 자동 선택 — 순서대로 내려가며 실패 이유를 전부 남긴다.
    std::string lastError;
    for (const Backend candidate : PreferredBackendOrder()) {
        DeviceFactory factory;
        {
            FactoryTable& t = Table();
            std::lock_guard<std::mutex> lock(t.mutex);
            factory = t.factories[static_cast<usize>(candidate)];
        }
        if (!factory) {
            ALICE_LOG_DEBUG(kChannel, "rhi.backend.rejected")
                .Msg("후보 백엔드가 이 빌드에 없다")
                .F("backend", ToString(candidate))
                .F("reason", "not compiled in");
            continue;
        }

        DeviceCreateInfo attempt = info;
        attempt.backend = candidate;

        Result<std::unique_ptr<IDevice>> device = factory(attempt);
        if (device.IsErr()) {
            lastError = device.Error().message;
            ALICE_LOG_WARN(kChannel, "rhi.backend.rejected")
                .Msg("후보 백엔드 생성에 실패해 다음으로 넘어간다")
                .F("backend", ToString(candidate))
                .F("reason", lastError);
            continue;
        }

        ALICE_LOG_INFO(kChannel, "rhi.backend.selected")
            .Msg("그래픽 백엔드를 자동 선택했다")
            .F("backend", ToString(candidate))
            .F("device", device.Value()->GetCapabilities().deviceName)
            .F("explicit", false);
        return device;
    }

    return MakeError("rhi.no_backend",
                     "쓸 수 있는 그래픽 백엔드가 없다",
                     lastError.empty()
                         ? "빌드에 백엔드가 하나도 포함되지 않았다. CMake 의 ALICE_RHI_* 옵션을 확인하라"
                         : Fmt("마지막 실패 이유: {}", lastError));
}

Result<std::unique_ptr<IDevice>> CreateBestDevice(bool enableValidation) {
    LinkAllBackends();
    DeviceCreateInfo info;
    info.backend          = Backend::Count;
    info.enableValidation = enableValidation;
    return CreateDevice(info);
}

void LinkAllBackends() noexcept {
    ForceLinkNullBackend();
#if ALICE_RHI_HAS_D3D11
    ForceLinkD3D11Backend();
#endif
#if ALICE_RHI_HAS_D3D12
    ForceLinkD3D12Backend();
#endif
#if ALICE_RHI_HAS_VULKAN
    ForceLinkVulkanBackend();
#endif
#if ALICE_RHI_HAS_METAL
    ForceLinkMetalBackend();
#endif
}

} // namespace alice::rhi
