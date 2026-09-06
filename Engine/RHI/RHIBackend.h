// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — RHI/RHIBackend.h
//
// 백엔드 선택. 요구사항은 "쉽고 간단하게 전 플랫폼 지원"이다.
//
// 그래서 콘텐츠와 게임 코드가 백엔드를 고르지 않는다. 프로젝트 문서에
//     renderer: { backend: auto }
// 라고 적으면 끝이고, 여기가 플랫폼과 드라이버를 보고 결정한다.
// 명시적으로 고르는 것은 디버깅과 벤치마크를 위한 예외 경로다.
//
// 선택 실패는 조용히 넘어가지 않는다. 왜 그 백엔드가 안 됐는지가 구조화 로그로 남는다:
//   rhi.backend.rejected  { backend: "d3d12", reason: "device does not support SM 6.6" }
// 이 한 줄이 있으면 AI 가 "왜 Vulkan 으로 떨어졌지?" 를 스스로 답할 수 있다.
#pragma once

#include "Foundation/Result.h"
#include "RHIDevice.h"

#include <functional>
#include <memory>
#include <vector>

namespace alice::rhi {

enum class Backend : u8 {
    Null = 0,   ///< 헤드리스. CI·테스트·문서 검증에 쓴다. 항상 성공한다
    D3D11,
    D3D12,
    Vulkan,
    Metal,
    Count
};

const char* ToString(Backend b) noexcept;
bool        ParseBackend(std::string_view name, Backend& out) noexcept;

struct DeviceCreateInfo {
    Backend backend = Backend::Null;
    bool    enableValidation = false;   ///< 디버그 레이어. 개발 빌드 기본 on
    bool    enableGpuTiming = true;
    bool    preferHighPerformanceAdapter = true;
    /// 특정 어댑터를 강제. 비우면 자동.
    std::string preferredAdapterName;
};

/// 백엔드 팩토리. 각 백엔드가 자기 자신을 등록한다.
using DeviceFactory = std::function<Result<std::unique_ptr<IDevice>>(const DeviceCreateInfo&)>;

/// 백엔드를 등록한다. 백엔드 .cpp 의 정적 초기화에서 호출한다.
void RegisterBackend(Backend backend, DeviceFactory factory);

/// 이 빌드에 실제로 들어 있는 백엔드 목록.
std::vector<Backend> AvailableBackends();
bool                 IsBackendAvailable(Backend backend) noexcept;

/// 이 플랫폼에서 시도할 순서. auto 선택이 이 순서를 따른다.
/// Windows: D3D12 → D3D11 → Vulkan → Null
/// macOS/iOS: Metal → Vulkan(MoltenVK) → Null
/// Android/Linux: Vulkan → Null
std::vector<Backend> PreferredBackendOrder();

/// 백엔드를 만든다. backend 가 Count 면 자동 선택한다.
/// 자동 선택은 PreferredBackendOrder 를 따라 내려가며, 실패할 때마다 이유를 로그로 남긴다.
Result<std::unique_ptr<IDevice>> CreateDevice(const DeviceCreateInfo& info);

/// 자동 선택 전용 헬퍼.
Result<std::unique_ptr<IDevice>> CreateBestDevice(bool enableValidation = false);

// ── 링커 보존 훅 ───────────────────────────────────────────────────────────
// 백엔드는 정적 초기화로 자기를 등록한다. 그런데 정적 라이브러리에서 아무도 참조하지
// 않는 번역 단위는 링커가 통째로 버린다 — 그러면 백엔드가 "조용히" 사라지고, 런타임에
// "쓸 수 있는 백엔드가 없다"는 엉뚱한 에러만 남는다. 이걸 한 번 부르면 그 일이 안 생긴다.
// (CMake 의 WHOLE_ARCHIVE 대신 이 방식을 쓰는 이유: 모든 툴체인에서 똑같이 동작한다)
void ForceLinkNullBackend() noexcept;
#if ALICE_RHI_HAS_D3D11
void ForceLinkD3D11Backend() noexcept;
#endif
#if ALICE_RHI_HAS_D3D12
void ForceLinkD3D12Backend() noexcept;
#endif
#if ALICE_RHI_HAS_VULKAN
void ForceLinkVulkanBackend() noexcept;
#endif
#if ALICE_RHI_HAS_METAL
void ForceLinkMetalBackend() noexcept;
#endif

/// 이 빌드에 포함된 모든 백엔드를 등록시킨다. 엔진 시작에서 한 번 부른다.
void LinkAllBackends() noexcept;

} // namespace alice::rhi
