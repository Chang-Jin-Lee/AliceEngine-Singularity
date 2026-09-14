// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — RHI/RHIDevice.h
//
// 백엔드가 구현해야 하는 인터페이스. 이 파일이 곧 "새 그래픽 API 를 붙이려면 무엇을
// 만들어야 하는가"의 목록이다. 짧을수록 좋다 — 짧아야 Metal, 콘솔, 다음 API 가 붙는다.
//
// 명시적으로 **넣지 않은 것들**과 그 이유:
//   • 배리어 / 리소스 상태 전이  → RenderGraph 가 자동 계산한다. 사람이 손으로 쓰면 반드시 틀린다
//   • 디스크립터 힙 관리         → 백엔드 내부 사정이다. 바인드리스면 인덱스만 보인다
//   • 메모리 할당자              → 백엔드가 VMA/D3D12MA 등을 알아서 쓴다
//   • 셰이더 컴파일              → Asset 파이프라인의 일이다. 여기는 바이트코드만 받는다
//
// 이 네 가지가 다른 엔진에서 이식을 어렵게 만드는 주범이다. 인터페이스 밖으로 밀어냈다.
#pragma once

#include "Foundation/Result.h"
#include "RHITypes.h"

#include <memory>
#include <vector>

namespace alice::rhi {

class ICommandList;

/// 프레임 하나에 대한 GPU 타임스탬프 결과.
struct GpuTimingResult {
    std::string name;
    u64         durationNs = 0;
};

class IDevice {
public:
    virtual ~IDevice() = default;

    virtual const Capabilities& GetCapabilities() const = 0;
    virtual const char*         BackendName() const = 0;

    // ── 리소스 ─────────────────────────────────────────────────────────────
    virtual Result<BufferHandle>   CreateBuffer(const BufferDesc& desc) = 0;
    virtual Result<TextureHandle>  CreateTexture(const TextureDesc& desc) = 0;
    virtual Result<SamplerHandle>  CreateSampler(const SamplerDesc& desc) = 0;
    virtual Result<ShaderHandle>   CreateShader(const ShaderDesc& desc) = 0;
    virtual Result<PipelineHandle> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
    virtual Result<PipelineHandle> CreateComputePipeline(const ComputePipelineDesc& desc) = 0;
    virtual Result<SwapchainHandle> CreateSwapchain(const SwapchainDesc& desc) = 0;

    virtual void Destroy(BufferHandle handle) = 0;
    virtual void Destroy(TextureHandle handle) = 0;
    virtual void Destroy(SamplerHandle handle) = 0;
    virtual void Destroy(ShaderHandle handle) = 0;
    virtual void Destroy(PipelineHandle handle) = 0;
    virtual void Destroy(SwapchainHandle handle) = 0;

    /// CPU 에서 쓸 수 있는 버퍼의 메모리를 연다. GpuOnly 버퍼에는 실패한다.
    virtual Result<void*> MapBuffer(BufferHandle handle) = 0;
    virtual void          UnmapBuffer(BufferHandle handle) = 0;

    /// 아무 때나 호출 가능한 업로드. 내부적으로 스테이징을 알아서 처리한다.
    virtual Status UploadBuffer(BufferHandle handle, const void* data, u64 size, u64 offset = 0) = 0;
    virtual Status UploadTexture(TextureHandle handle, const void* data, usize size,
                                 u32 mipLevel = 0, u32 arraySlice = 0) = 0;

    // ── 프레임 ─────────────────────────────────────────────────────────────
    virtual Status BeginFrame(u64 frameIndex) = 0;
    virtual Status EndFrame() = 0;
    virtual Status Present(SwapchainHandle swapchain) = 0;
    virtual Status ResizeSwapchain(SwapchainHandle swapchain, u32 width, u32 height) = 0;
    virtual Result<TextureHandle> AcquireBackbuffer(SwapchainHandle swapchain) = 0;

    /// 이 프레임의 명령 목록을 하나 얻는다. 스레드마다 따로 얻어 병렬 기록할 수 있다.
    virtual ICommandList* AcquireCommandList(const char* debugName) = 0;
    virtual Status        Submit(ICommandList* commandList) = 0;

    /// GPU 가 밀린 작업을 다 끝낼 때까지 기다린다. 종료와 리사이즈에만 쓴다.
    virtual void WaitIdle() = 0;

    // ── 계측 (요구 2) ──────────────────────────────────────────────────────
    /// 해석이 끝난 GPU 타이밍을 가져간다. 몇 프레임 늦게 나온다.
    /// Profiler::SubmitGpuZone 으로 흘려보내면 CPU 존과 같은 축에 놓인다.
    virtual std::vector<GpuTimingResult> CollectGpuTimings(u64& outFrameIndex) = 0;

    /// 살아 있는 리소스 수. 누수를 이 값의 추이로 잡는다.
    virtual usize LiveResourceCount() const = 0;
};

/// 명령 기록기. 백엔드가 즉시 모드(DX11)든 목록 모드(DX12/VK/Metal)든 같은 얼굴을 보인다.
class ICommandList {
public:
    virtual ~ICommandList() = default;

    virtual void Begin() = 0;
    virtual void End() = 0;

    /// 프로파일러·RenderDoc·PIX 에 그대로 보이는 구간 표시.
    virtual void PushDebugGroup(const char* name) = 0;
    virtual void PopDebugGroup() = 0;

    virtual void BeginRenderPass(const RenderPassDesc& desc) = 0;
    virtual void EndRenderPass() = 0;

    virtual void SetViewport(const Viewport& viewport) = 0;
    virtual void SetScissor(const ScissorRect& scissor) = 0;
    virtual void SetPipeline(PipelineHandle pipeline) = 0;

    virtual void SetVertexBuffer(u32 slot, BufferHandle buffer, u64 offset = 0) = 0;
    virtual void SetIndexBuffer(BufferHandle buffer, Format format, u64 offset = 0) = 0;

    /// 바인딩. 바인드리스 백엔드에서는 인덱스만 넘어가고, 아니면 슬롯에 꽂힌다.
    virtual void BindBuffer(ShaderStage stage, u32 slot, BufferHandle buffer) = 0;
    virtual void BindTexture(ShaderStage stage, u32 slot, TextureHandle texture) = 0;
    virtual void BindSampler(ShaderStage stage, u32 slot, SamplerHandle sampler) = 0;
    /// 작은 상수. 백엔드가 push constant / root constant 로 옮긴다.
    virtual void SetPushConstants(ShaderStage stage, const void* data, u32 size) = 0;

    virtual void Draw(u32 vertexCount, u32 instanceCount = 1,
                      u32 firstVertex = 0, u32 firstInstance = 0) = 0;
    virtual void DrawIndexed(u32 indexCount, u32 instanceCount = 1, u32 firstIndex = 0,
                             i32 vertexOffset = 0, u32 firstInstance = 0) = 0;
    virtual void DrawIndirect(BufferHandle argsBuffer, u64 offset, u32 drawCount) = 0;

    virtual void Dispatch(u32 groupsX, u32 groupsY, u32 groupsZ) = 0;
    virtual void DispatchIndirect(BufferHandle argsBuffer, u64 offset) = 0;

    virtual void CopyBuffer(BufferHandle dst, u64 dstOffset,
                            BufferHandle src, u64 srcOffset, u64 size) = 0;
    virtual void CopyTexture(TextureHandle dst, TextureHandle src) = 0;

    /// GPU 타임스탬프 구간. 이름은 정적 문자열이어야 한다.
    virtual void BeginGpuZone(const char* name) = 0;
    virtual void EndGpuZone() = 0;
};

/// 스코프 기반 디버그 그룹. 매크로로 쓴다.
class ScopedDebugGroup {
public:
    ScopedDebugGroup(ICommandList& list, const char* name) : m_list(list) {
        m_list.PushDebugGroup(name);
    }
    ~ScopedDebugGroup() { m_list.PopDebugGroup(); }
    ScopedDebugGroup(const ScopedDebugGroup&) = delete;
    ScopedDebugGroup& operator=(const ScopedDebugGroup&) = delete;
private:
    ICommandList& m_list;
};

/// GPU 존 + 디버그 그룹을 한 번에. 렌더 패스마다 이걸 쓴다.
class ScopedGpuZone {
public:
    ScopedGpuZone(ICommandList& list, const char* name) : m_list(list) {
        m_list.PushDebugGroup(name);
        m_list.BeginGpuZone(name);
    }
    ~ScopedGpuZone() {
        m_list.EndGpuZone();
        m_list.PopDebugGroup();
    }
    ScopedGpuZone(const ScopedGpuZone&) = delete;
    ScopedGpuZone& operator=(const ScopedGpuZone&) = delete;
private:
    ICommandList& m_list;
};

} // namespace alice::rhi

#define ALICE_GPU_ZONE(cmdList, nameLiteral) \
    ::alice::rhi::ScopedGpuZone ALICE_UNIQUE(aliceGpuZone_)((cmdList), (nameLiteral))
