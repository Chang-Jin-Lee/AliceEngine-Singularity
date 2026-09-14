// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — RHI/NullDevice.cpp
//
// 헤드리스 백엔드. GPU 없이 RHI 계약 전부를 구현한다.
//
// 장난감이 아니다. 이 백엔드가 담당하는 일:
//   • CI 에서 렌더 코드 경로를 실제로 실행한다 (GPU 없는 러너에서도)
//   • 핸들 수명, 사용 플래그, 상태 순서를 **검증**한다. 진짜 백엔드보다 엄격하다
//   • 드로우콜·디스패치·업로드량을 세어 회귀 테스트의 기준값을 만든다
//   • 콘텐츠 문서만 검증하고 싶을 때 창을 띄우지 않고 엔진을 띄운다
//
// 실전 검증기를 겸하므로, 여기서 통과하지 못하는 코드는 D3D12/Vulkan 에서도 통과하지 못한다.
#include "Foundation/Log.h"
#include "Foundation/Profiler.h"
#include "Foundation/StringUtil.h"
#include "RHIBackend.h"

#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

namespace alice::rhi {
namespace {

constexpr const char* kChannel = "rhi";

/// 세대 검사가 붙은 핸들 풀. 해제 후 재사용을 즉시 잡아낸다.
template <typename T, typename HandleType>
class HandlePool {
public:
    HandleType Create(T value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        u32 index;
        if (!m_free.empty()) {
            index = m_free.back();
            m_free.pop_back();
            m_slots[index].value = std::move(value);
            m_slots[index].alive = true;
        } else {
            index = static_cast<u32>(m_slots.size()) + 1;   // 0 은 무효 핸들로 예약
            m_slots.push_back(Slot{std::move(value), 1, true});
        }
        HandleType h;
        h.index      = index;
        h.generation = m_slots[index - 1].generation;
        ++m_liveCount;
        return h;
    }

    T* Get(HandleType handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Slot* slot = Resolve(handle);
        return slot ? &slot->value : nullptr;
    }

    bool Destroy(HandleType handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Slot* slot = Resolve(handle);
        if (!slot) return false;
        slot->alive = false;
        ++slot->generation;              // 옛 핸들을 즉시 무효화한다
        m_free.push_back(handle.index);
        --m_liveCount;
        return true;
    }

    usize LiveCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_liveCount;
    }

    /// 살아 있는 것들의 이름. 누수 보고에 쓴다.
    template <typename Fn>
    void ForEachLive(Fn&& fn) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const Slot& slot : m_slots) {
            if (slot.alive) fn(slot.value);
        }
    }

private:
    struct Slot {
        T    value;
        u16  generation = 1;
        bool alive = false;
    };

    Slot* Resolve(HandleType handle) {
        if (handle.index == 0 || handle.index > m_slots.size()) return nullptr;
        Slot& slot = m_slots[handle.index - 1];
        if (!slot.alive || slot.generation != handle.generation) return nullptr;
        return &slot;
    }

    mutable std::mutex m_mutex;
    std::vector<Slot>  m_slots;
    std::vector<u32>   m_free;
    usize              m_liveCount = 0;
};

struct NullBuffer {
    BufferDesc        desc;
    std::vector<u8>   storage;   ///< CPU 접근 버퍼만 실제 메모리를 갖는다
    bool              mapped = false;
};

struct NullTexture  { TextureDesc desc; };
struct NullSampler  { SamplerDesc desc; };
struct NullShader   { ShaderDesc  desc; };
struct NullPipeline { std::string debugName; bool isCompute = false; };
struct NullSwapchain {
    SwapchainDesc desc;
    TextureHandle backbuffer;
};

/// 한 프레임 동안 일어난 일. 회귀 테스트가 이 숫자를 본다.
struct FrameStats {
    u64 drawCalls = 0;
    u64 dispatches = 0;
    u64 renderPasses = 0;
    u64 pipelineChanges = 0;
    u64 uploadedBytes = 0;
    u64 vertices = 0;

    void Reset() { *this = FrameStats{}; }
};

class NullDevice;

class NullCommandList final : public ICommandList {
public:
    NullCommandList(NullDevice& device, std::string debugName)
        : m_device(device), m_debugName(std::move(debugName)) {}

    void Begin() override;
    void End() override;
    void PushDebugGroup(const char* name) override;
    void PopDebugGroup() override;
    void BeginRenderPass(const RenderPassDesc& desc) override;
    void EndRenderPass() override;
    void SetViewport(const Viewport& viewport) override { m_viewport = viewport; }
    void SetScissor(const ScissorRect& scissor) override { m_scissor = scissor; }
    void SetPipeline(PipelineHandle pipeline) override;
    void SetVertexBuffer(u32 slot, BufferHandle buffer, u64 offset) override;
    void SetIndexBuffer(BufferHandle buffer, Format format, u64 offset) override;
    void BindBuffer(ShaderStage stage, u32 slot, BufferHandle buffer) override;
    void BindTexture(ShaderStage stage, u32 slot, TextureHandle texture) override;
    void BindSampler(ShaderStage stage, u32 slot, SamplerHandle sampler) override;
    void SetPushConstants(ShaderStage stage, const void* data, u32 size) override;
    void Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) override;
    void DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex,
                     i32 vertexOffset, u32 firstInstance) override;
    void DrawIndirect(BufferHandle argsBuffer, u64 offset, u32 drawCount) override;
    void Dispatch(u32 groupsX, u32 groupsY, u32 groupsZ) override;
    void DispatchIndirect(BufferHandle argsBuffer, u64 offset) override;
    void CopyBuffer(BufferHandle dst, u64 dstOffset, BufferHandle src, u64 srcOffset, u64 size) override;
    void CopyTexture(TextureHandle dst, TextureHandle src) override;
    void BeginGpuZone(const char* name) override;
    void EndGpuZone() override;

    bool InRenderPass() const noexcept { return m_inRenderPass; }
    bool IsOpen() const noexcept { return m_open; }
    const std::string& DebugName() const noexcept { return m_debugName; }

private:
    /// 계약 위반을 구조화 로그로 남긴다. 진짜 백엔드에서는 크래시가 될 것들이다.
    void Violation(const char* event, std::string message);

    NullDevice&  m_device;
    std::string  m_debugName;
    bool         m_open = false;
    bool         m_inRenderPass = false;
    u32          m_debugDepth = 0;
    u32          m_gpuZoneDepth = 0;
    PipelineHandle m_pipeline;
    Viewport     m_viewport;
    ScissorRect  m_scissor;
    std::vector<const char*> m_gpuZoneStack;
};

class NullDevice final : public IDevice {
public:
    explicit NullDevice(const DeviceCreateInfo& info) : m_validation(info.enableValidation) {
        m_caps.deviceName    = "Alice Null Device";
        m_caps.driverVersion = ALICE_VERSION_STRING;
        m_caps.videoMemoryBytes = 0;

        // Null 은 "가장 관대한 하드웨어"인 척한다. 그래야 콘텐츠가 기능 분기를 전부 밟아본다.
        m_caps.bindlessResources   = true;
        m_caps.computeShaders      = true;
        m_caps.geometryShaders     = true;
        m_caps.tessellation        = true;
        m_caps.meshShaders         = true;
        m_caps.rayTracing          = true;
        m_caps.variableRateShading = true;
        m_caps.conservativeRaster  = true;
        m_caps.timestampQueries    = true;
        m_caps.pipelineStatistics  = true;
    }

    ~NullDevice() override { ReportLeaks(); }

    const Capabilities& GetCapabilities() const override { return m_caps; }
    const char* BackendName() const override { return "null"; }

    // ── 리소스 ─────────────────────────────────────────────────────────────
    Result<BufferHandle> CreateBuffer(const BufferDesc& desc) override {
        if (desc.size == 0) {
            return MakeError("rhi.invalid_buffer_size", "크기가 0인 버퍼는 만들 수 없다",
                             "BufferDesc::size 를 설정하라");
        }
        if (m_validation && desc.debugName.empty()) {
            ALICE_LOG_WARN(kChannel, "rhi.resource.unnamed")
                .Msg("이름 없는 버퍼를 만들었다. 프로파일러에서 추적할 수 없다")
                .F("size", static_cast<i64>(desc.size));
        }

        NullBuffer buffer;
        buffer.desc = desc;
        if (desc.access != MemoryAccess::GpuOnly) {
            buffer.storage.resize(static_cast<usize>(desc.size));
        }
        const BufferHandle handle = m_buffers.Create(std::move(buffer));

        ALICE_LOG_TRACE(kChannel, "rhi.buffer.created")
            .F("handle", static_cast<i64>(handle.Key()))
            .F("name", desc.debugName)
            .F("size", static_cast<i64>(desc.size));
        return handle;
    }

    Result<TextureHandle> CreateTexture(const TextureDesc& desc) override {
        if (desc.width == 0 || desc.height == 0) {
            return MakeError("rhi.invalid_texture_size", "가로 또는 세로가 0인 텍스처는 만들 수 없다");
        }
        if (desc.width > m_caps.maxTextureDimension2D || desc.height > m_caps.maxTextureDimension2D) {
            return MakeError("rhi.texture_too_large",
                             Fmt("텍스처가 {}x{} 로 한계 {} 를 넘는다",
                                    desc.width, desc.height, m_caps.maxTextureDimension2D));
        }
        NullTexture texture;
        texture.desc = desc;
        return m_textures.Create(std::move(texture));
    }

    Result<SamplerHandle> CreateSampler(const SamplerDesc& desc) override {
        if (desc.maxAnisotropy > m_caps.maxAnisotropy) {
            return MakeError("rhi.anisotropy_too_high",
                             Fmt("이방성 {} 는 한계 {} 를 넘는다",
                                    desc.maxAnisotropy, m_caps.maxAnisotropy));
        }
        NullSampler sampler;
        sampler.desc = desc;
        return m_samplers.Create(std::move(sampler));
    }

    Result<ShaderHandle> CreateShader(const ShaderDesc& desc) override {
        // Null 백엔드는 바이트코드를 해석하지 않지만, 비어 있으면 잡아준다.
        // 셰이더 컴파일이 조용히 실패해 빈 바이트코드가 넘어오는 사고가 흔하다.
        if (desc.bytecodeSize == 0) {
            return MakeError("rhi.empty_shader",
                             Fmt("셰이더 '{}' 의 바이트코드가 비어 있다", desc.debugName),
                             "셰이더 컴파일이 성공했는지 확인하라");
        }
        NullShader shader;
        shader.desc = desc;
        return m_shaders.Create(std::move(shader));
    }

    Result<PipelineHandle> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override {
        if (!m_shaders.Get(desc.vertexShader)) {
            return MakeError("rhi.invalid_shader_handle",
                             Fmt("파이프라인 '{}' 의 버텍스 셰이더 핸들이 유효하지 않다",
                                    desc.debugName),
                             "셰이더를 만든 뒤 파괴하지 않았는지 확인하라");
        }
        if (desc.renderTargetCount > m_caps.maxColorAttachments) {
            return MakeError("rhi.too_many_render_targets",
                             Fmt("렌더 타깃 {}개는 한계 {}개를 넘는다",
                                    desc.renderTargetCount, m_caps.maxColorAttachments));
        }
        NullPipeline pipeline;
        pipeline.debugName = desc.debugName;
        return m_pipelines.Create(std::move(pipeline));
    }

    Result<PipelineHandle> CreateComputePipeline(const ComputePipelineDesc& desc) override {
        if (!m_shaders.Get(desc.computeShader)) {
            return MakeError("rhi.invalid_shader_handle",
                             Fmt("컴퓨트 파이프라인 '{}' 의 셰이더 핸들이 유효하지 않다",
                                    desc.debugName));
        }
        NullPipeline pipeline;
        pipeline.debugName = desc.debugName;
        pipeline.isCompute = true;
        return m_pipelines.Create(std::move(pipeline));
    }

    Result<SwapchainHandle> CreateSwapchain(const SwapchainDesc& desc) override {
        TextureDesc backbufferDesc;
        backbufferDesc.width     = desc.width ? desc.width : 1;
        backbufferDesc.height    = desc.height ? desc.height : 1;
        backbufferDesc.format    = desc.format;
        backbufferDesc.usage     = ResourceUsage::RenderTarget | ResourceUsage::Present;
        backbufferDesc.debugName = desc.debugName.empty() ? "backbuffer"
                                                          : desc.debugName + ".backbuffer";

        Result<TextureHandle> backbuffer = CreateTexture(backbufferDesc);
        if (backbuffer.IsErr()) return backbuffer.Error();

        NullSwapchain swapchain;
        swapchain.desc       = desc;
        swapchain.backbuffer = *backbuffer;
        return m_swapchains.Create(std::move(swapchain));
    }

    void Destroy(BufferHandle handle) override    { WarnIfStale(m_buffers.Destroy(handle), "buffer"); }
    void Destroy(TextureHandle handle) override   { WarnIfStale(m_textures.Destroy(handle), "texture"); }
    void Destroy(SamplerHandle handle) override   { WarnIfStale(m_samplers.Destroy(handle), "sampler"); }
    void Destroy(ShaderHandle handle) override    { WarnIfStale(m_shaders.Destroy(handle), "shader"); }
    void Destroy(PipelineHandle handle) override  { WarnIfStale(m_pipelines.Destroy(handle), "pipeline"); }
    void Destroy(SwapchainHandle handle) override {
        if (NullSwapchain* swapchain = m_swapchains.Get(handle)) {
            m_textures.Destroy(swapchain->backbuffer);
        }
        WarnIfStale(m_swapchains.Destroy(handle), "swapchain");
    }

    Result<void*> MapBuffer(BufferHandle handle) override {
        NullBuffer* buffer = m_buffers.Get(handle);
        if (!buffer) {
            return MakeError("rhi.invalid_handle", "유효하지 않은 버퍼 핸들이다",
                             "이미 Destroy 한 핸들일 수 있다");
        }
        if (buffer->desc.access == MemoryAccess::GpuOnly) {
            return MakeError("rhi.buffer_not_mappable",
                             Fmt("버퍼 '{}' 는 GpuOnly 라 매핑할 수 없다", buffer->desc.debugName),
                             "BufferDesc::access 를 CpuToGpu 또는 GpuToCpu 로 만들거나 "
                             "UploadBuffer 를 쓰라");
        }
        if (buffer->mapped) {
            return MakeError("rhi.buffer_already_mapped",
                             Fmt("버퍼 '{}' 가 이미 매핑되어 있다", buffer->desc.debugName));
        }
        buffer->mapped = true;
        return static_cast<void*>(buffer->storage.data());
    }

    void UnmapBuffer(BufferHandle handle) override {
        if (NullBuffer* buffer = m_buffers.Get(handle)) buffer->mapped = false;
    }

    Status UploadBuffer(BufferHandle handle, const void* data, u64 size, u64 offset) override {
        NullBuffer* buffer = m_buffers.Get(handle);
        if (!buffer) return MakeError("rhi.invalid_handle", "유효하지 않은 버퍼 핸들이다");
        if (offset + size > buffer->desc.size) {
            return MakeError("rhi.upload_out_of_range",
                             Fmt("업로드 범위 {}+{} 가 버퍼 크기 {} 를 넘는다",
                                    static_cast<u64>(offset), static_cast<u64>(size),
                                    static_cast<u64>(buffer->desc.size)));
        }
        if (!buffer->storage.empty() && data) {
            std::memcpy(buffer->storage.data() + offset, data, static_cast<usize>(size));
        }
        m_frameStats.uploadedBytes += size;
        return Status::Ok();
    }

    Status UploadTexture(TextureHandle handle, const void* data, usize size,
                         u32 mipLevel, u32 arraySlice) override {
        ALICE_UNUSED(data);
        NullTexture* texture = m_textures.Get(handle);
        if (!texture) return MakeError("rhi.invalid_handle", "유효하지 않은 텍스처 핸들이다");
        if (mipLevel >= texture->desc.mipLevels) {
            return MakeError("rhi.mip_out_of_range",
                             Fmt("밉 {} 는 이 텍스처의 밉 수 {} 를 넘는다",
                                    mipLevel, texture->desc.mipLevels));
        }
        if (arraySlice >= texture->desc.arraySize) {
            return MakeError("rhi.slice_out_of_range",
                             Fmt("배열 슬라이스 {} 는 크기 {} 를 넘는다",
                                    arraySlice, texture->desc.arraySize));
        }
        m_frameStats.uploadedBytes += size;
        return Status::Ok();
    }

    // ── 프레임 ─────────────────────────────────────────────────────────────
    Status BeginFrame(u64 frameIndex) override {
        if (m_frameOpen) {
            return MakeError("rhi.frame_already_open",
                             "EndFrame 없이 BeginFrame 을 다시 불렀다");
        }
        m_frameOpen  = true;
        m_frameIndex = frameIndex;
        m_frameStats.Reset();
        return Status::Ok();
    }

    Status EndFrame() override {
        if (!m_frameOpen) {
            return MakeError("rhi.frame_not_open", "BeginFrame 없이 EndFrame 을 불렀다");
        }
        m_frameOpen = false;

        ALICE_LOG_TRACE(kChannel, "rhi.frame.stats")
            .F("frame", static_cast<i64>(m_frameIndex))
            .F("draws", static_cast<i64>(m_frameStats.drawCalls))
            .F("dispatches", static_cast<i64>(m_frameStats.dispatches))
            .F("passes", static_cast<i64>(m_frameStats.renderPasses))
            .F("pipelineChanges", static_cast<i64>(m_frameStats.pipelineChanges))
            .F("uploadedBytes", static_cast<i64>(m_frameStats.uploadedBytes));

        m_lastFrameStats = m_frameStats;
        return Status::Ok();
    }

    Status Present(SwapchainHandle swapchain) override {
        if (!m_swapchains.Get(swapchain)) {
            return MakeError("rhi.invalid_handle", "유효하지 않은 스왑체인 핸들이다");
        }
        return Status::Ok();
    }

    Status ResizeSwapchain(SwapchainHandle handle, u32 width, u32 height) override {
        NullSwapchain* swapchain = m_swapchains.Get(handle);
        if (!swapchain) return MakeError("rhi.invalid_handle", "유효하지 않은 스왑체인 핸들이다");
        swapchain->desc.width  = width;
        swapchain->desc.height = height;
        return Status::Ok();
    }

    Result<TextureHandle> AcquireBackbuffer(SwapchainHandle handle) override {
        NullSwapchain* swapchain = m_swapchains.Get(handle);
        if (!swapchain) return MakeError("rhi.invalid_handle", "유효하지 않은 스왑체인 핸들이다");
        return swapchain->backbuffer;
    }

    ICommandList* AcquireCommandList(const char* debugName) override {
        std::lock_guard<std::mutex> lock(m_listMutex);
        m_commandLists.push_back(
            std::make_unique<NullCommandList>(*this, debugName ? debugName : "unnamed"));
        return m_commandLists.back().get();
    }

    Status Submit(ICommandList* commandList) override {
        auto* list = static_cast<NullCommandList*>(commandList);
        if (!list) return MakeError("rhi.null_command_list", "널 명령 목록을 제출했다");
        if (list->IsOpen()) {
            return MakeError("rhi.command_list_open",
                             Fmt("명령 목록 '{}' 을(를) End() 없이 제출했다", list->DebugName()));
        }
        return Status::Ok();
    }

    void WaitIdle() override {}

    std::vector<GpuTimingResult> CollectGpuTimings(u64& outFrameIndex) override {
        outFrameIndex = m_frameIndex;
        std::lock_guard<std::mutex> lock(m_timingMutex);
        std::vector<GpuTimingResult> out = std::move(m_pendingTimings);
        m_pendingTimings.clear();
        return out;
    }

    usize LiveResourceCount() const override {
        return m_buffers.LiveCount() + m_textures.LiveCount() + m_samplers.LiveCount() +
               m_shaders.LiveCount() + m_pipelines.LiveCount() + m_swapchains.LiveCount();
    }

    // ── 명령 목록에서 쓰는 내부 API ────────────────────────────────────────
    FrameStats& Stats() noexcept { return m_frameStats; }
    const FrameStats& LastFrameStats() const noexcept { return m_lastFrameStats; }
    bool Validation() const noexcept { return m_validation; }
    u64  FrameIndex() const noexcept { return m_frameIndex; }

    bool IsValid(BufferHandle h)   { return m_buffers.Get(h) != nullptr; }
    bool IsValid(TextureHandle h)  { return m_textures.Get(h) != nullptr; }
    bool IsValid(SamplerHandle h)  { return m_samplers.Get(h) != nullptr; }
    bool IsValid(PipelineHandle h) { return m_pipelines.Get(h) != nullptr; }

    void RecordGpuTiming(const char* name, u64 durationNs) {
        std::lock_guard<std::mutex> lock(m_timingMutex);
        m_pendingTimings.push_back(GpuTimingResult{name ? name : "gpu", durationNs});
    }

private:
    void WarnIfStale(bool ok, const char* kind) {
        if (ok) return;
        ALICE_LOG_ERROR(kChannel, "rhi.destroy.invalid_handle")
            .Msg("이미 파괴됐거나 유효하지 않은 핸들을 파괴하려 했다")
            .F("kind", kind);
    }

    void ReportLeaks() {
        const usize live = LiveResourceCount();
        if (live == 0) return;

        ALICE_LOG_ERROR(kChannel, "rhi.leak.detected")
            .Msg("디바이스 종료 시점에 살아 있는 리소스가 있다")
            .F("count", static_cast<i64>(live))
            .F("buffers", static_cast<i64>(m_buffers.LiveCount()))
            .F("textures", static_cast<i64>(m_textures.LiveCount()))
            .F("samplers", static_cast<i64>(m_samplers.LiveCount()))
            .F("shaders", static_cast<i64>(m_shaders.LiveCount()))
            .F("pipelines", static_cast<i64>(m_pipelines.LiveCount()))
            .F("swapchains", static_cast<i64>(m_swapchains.LiveCount()));

        // 이름을 함께 남긴다. 개수만으로는 어느 것이 샜는지 알 수 없다.
        m_buffers.ForEachLive([](const NullBuffer& b) {
            ALICE_LOG_ERROR(kChannel, "rhi.leak.buffer").F("name", b.desc.debugName);
        });
        m_textures.ForEachLive([](const NullTexture& t) {
            ALICE_LOG_ERROR(kChannel, "rhi.leak.texture").F("name", t.desc.debugName);
        });
    }

    Capabilities m_caps;
    bool         m_validation = false;

    HandlePool<NullBuffer, BufferHandle>       m_buffers;
    HandlePool<NullTexture, TextureHandle>     m_textures;
    HandlePool<NullSampler, SamplerHandle>     m_samplers;
    HandlePool<NullShader, ShaderHandle>       m_shaders;
    HandlePool<NullPipeline, PipelineHandle>   m_pipelines;
    HandlePool<NullSwapchain, SwapchainHandle> m_swapchains;

    std::mutex                                   m_listMutex;
    std::vector<std::unique_ptr<NullCommandList>> m_commandLists;

    std::mutex                   m_timingMutex;
    std::vector<GpuTimingResult> m_pendingTimings;

    bool       m_frameOpen = false;
    u64        m_frameIndex = 0;
    FrameStats m_frameStats;
    FrameStats m_lastFrameStats;
};

// ── NullCommandList 구현 ───────────────────────────────────────────────────
void NullCommandList::Violation(const char* event, std::string message) {
    ALICE_LOG_ERROR(kChannel, event)
        .Msg(std::move(message))
        .F("commandList", m_debugName)
        .F("frame", static_cast<i64>(m_device.FrameIndex()));
}

void NullCommandList::Begin() {
    if (m_open) Violation("rhi.cmd.double_begin", "이미 열린 명령 목록에 Begin 을 다시 불렀다");
    m_open         = true;
    m_inRenderPass = false;
    m_debugDepth   = 0;
    m_gpuZoneDepth = 0;
    m_gpuZoneStack.clear();
}

void NullCommandList::End() {
    if (!m_open) Violation("rhi.cmd.end_without_begin", "Begin 없이 End 를 불렀다");
    if (m_inRenderPass) {
        Violation("rhi.cmd.unclosed_render_pass", "렌더 패스를 닫지 않고 명령 목록을 끝냈다");
    }
    if (m_debugDepth != 0) {
        Violation("rhi.cmd.unbalanced_debug_group",
                  Fmt("디버그 그룹이 {}개 열린 채로 끝났다", m_debugDepth));
    }
    if (m_gpuZoneDepth != 0) {
        Violation("rhi.cmd.unbalanced_gpu_zone",
                  Fmt("GPU 존이 {}개 열린 채로 끝났다", m_gpuZoneDepth));
    }
    m_open = false;
}

void NullCommandList::PushDebugGroup(const char* name) {
    ALICE_UNUSED(name);
    ++m_debugDepth;
}

void NullCommandList::PopDebugGroup() {
    if (m_debugDepth == 0) {
        Violation("rhi.cmd.debug_group_underflow", "열리지 않은 디버그 그룹을 닫았다");
        return;
    }
    --m_debugDepth;
}

void NullCommandList::BeginRenderPass(const RenderPassDesc& desc) {
    if (m_inRenderPass) {
        Violation("rhi.cmd.nested_render_pass",
                  Fmt("렌더 패스 '{}' 를 이전 패스를 닫지 않고 시작했다", desc.debugName));
    }
    for (u32 i = 0; i < desc.colorTargetCount; ++i) {
        if (!m_device.IsValid(desc.colorTargets[i].texture)) {
            Violation("rhi.cmd.invalid_render_target",
                      Fmt("렌더 패스 '{}' 의 컬러 타깃 {} 이(가) 유효하지 않다",
                             desc.debugName, i));
        }
    }
    if (desc.hasDepth && !m_device.IsValid(desc.depthTarget.texture)) {
        Violation("rhi.cmd.invalid_depth_target",
                  Fmt("렌더 패스 '{}' 의 뎁스 타깃이 유효하지 않다", desc.debugName));
    }
    m_inRenderPass = true;
    ++m_device.Stats().renderPasses;
}

void NullCommandList::EndRenderPass() {
    if (!m_inRenderPass) {
        Violation("rhi.cmd.render_pass_underflow", "시작하지 않은 렌더 패스를 닫았다");
        return;
    }
    m_inRenderPass = false;
}

void NullCommandList::SetPipeline(PipelineHandle pipeline) {
    if (!m_device.IsValid(pipeline)) {
        Violation("rhi.cmd.invalid_pipeline", "유효하지 않은 파이프라인 핸들을 설정했다");
        return;
    }
    if (m_pipeline != pipeline) {
        m_pipeline = pipeline;
        ++m_device.Stats().pipelineChanges;
    }
}

void NullCommandList::SetVertexBuffer(u32 slot, BufferHandle buffer, u64 offset) {
    ALICE_UNUSED(slot);
    ALICE_UNUSED(offset);
    if (!m_device.IsValid(buffer)) {
        Violation("rhi.cmd.invalid_vertex_buffer", "유효하지 않은 버텍스 버퍼를 설정했다");
    }
}

void NullCommandList::SetIndexBuffer(BufferHandle buffer, Format format, u64 offset) {
    ALICE_UNUSED(offset);
    if (!m_device.IsValid(buffer)) {
        Violation("rhi.cmd.invalid_index_buffer", "유효하지 않은 인덱스 버퍼를 설정했다");
    }
    if (format != Format::R16Uint && format != Format::R32Uint) {
        Violation("rhi.cmd.bad_index_format",
                  Fmt("인덱스 버퍼 포맷은 r16_uint 또는 r32_uint 여야 하는데 {} 가 왔다",
                         ToString(format)));
    }
}

void NullCommandList::BindBuffer(ShaderStage stage, u32 slot, BufferHandle buffer) {
    ALICE_UNUSED(stage);
    ALICE_UNUSED(slot);
    if (!m_device.IsValid(buffer)) {
        Violation("rhi.cmd.invalid_bound_buffer", "유효하지 않은 버퍼를 바인딩했다");
    }
}

void NullCommandList::BindTexture(ShaderStage stage, u32 slot, TextureHandle texture) {
    ALICE_UNUSED(stage);
    ALICE_UNUSED(slot);
    if (!m_device.IsValid(texture)) {
        Violation("rhi.cmd.invalid_bound_texture", "유효하지 않은 텍스처를 바인딩했다");
    }
}

void NullCommandList::BindSampler(ShaderStage stage, u32 slot, SamplerHandle sampler) {
    ALICE_UNUSED(stage);
    ALICE_UNUSED(slot);
    if (!m_device.IsValid(sampler)) {
        Violation("rhi.cmd.invalid_bound_sampler", "유효하지 않은 샘플러를 바인딩했다");
    }
}

void NullCommandList::SetPushConstants(ShaderStage stage, const void* data, u32 size) {
    ALICE_UNUSED(stage);
    ALICE_UNUSED(data);
    // 128바이트는 Vulkan 이 보장하는 최소 push constant 크기다. 넘으면 이식성이 깨진다.
    if (size > 128) {
        Violation("rhi.cmd.push_constants_too_large",
                  Fmt("푸시 상수 {}바이트는 이식 가능한 한계 128바이트를 넘는다", size));
    }
}

void NullCommandList::Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) {
    ALICE_UNUSED(firstVertex);
    ALICE_UNUSED(firstInstance);
    if (!m_inRenderPass) {
        Violation("rhi.cmd.draw_outside_pass", "렌더 패스 밖에서 Draw 를 불렀다");
        return;
    }
    if (!m_pipeline.IsValid()) {
        Violation("rhi.cmd.draw_without_pipeline", "파이프라인 없이 Draw 를 불렀다");
        return;
    }
    ++m_device.Stats().drawCalls;
    m_device.Stats().vertices += static_cast<u64>(vertexCount) * instanceCount;
}

void NullCommandList::DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex,
                                  i32 vertexOffset, u32 firstInstance) {
    ALICE_UNUSED(firstIndex);
    ALICE_UNUSED(vertexOffset);
    ALICE_UNUSED(firstInstance);
    Draw(indexCount, instanceCount, 0, 0);
}

void NullCommandList::DrawIndirect(BufferHandle argsBuffer, u64 offset, u32 drawCount) {
    ALICE_UNUSED(offset);
    if (!m_device.IsValid(argsBuffer)) {
        Violation("rhi.cmd.invalid_indirect_buffer", "유효하지 않은 간접 인자 버퍼를 썼다");
        return;
    }
    m_device.Stats().drawCalls += drawCount;
}

void NullCommandList::Dispatch(u32 groupsX, u32 groupsY, u32 groupsZ) {
    if (m_inRenderPass) {
        Violation("rhi.cmd.dispatch_inside_pass",
                  "렌더 패스 안에서 Dispatch 를 불렀다. 컴퓨트는 패스 밖에서 한다");
    }
    if (!m_pipeline.IsValid()) {
        Violation("rhi.cmd.dispatch_without_pipeline", "파이프라인 없이 Dispatch 를 불렀다");
        return;
    }
    ALICE_UNUSED(groupsX);
    ALICE_UNUSED(groupsY);
    ALICE_UNUSED(groupsZ);
    ++m_device.Stats().dispatches;
}

void NullCommandList::DispatchIndirect(BufferHandle argsBuffer, u64 offset) {
    ALICE_UNUSED(offset);
    if (!m_device.IsValid(argsBuffer)) {
        Violation("rhi.cmd.invalid_indirect_buffer", "유효하지 않은 간접 인자 버퍼를 썼다");
        return;
    }
    ++m_device.Stats().dispatches;
}

void NullCommandList::CopyBuffer(BufferHandle dst, u64 dstOffset,
                                 BufferHandle src, u64 srcOffset, u64 size) {
    ALICE_UNUSED(dstOffset);
    ALICE_UNUSED(srcOffset);
    if (!m_device.IsValid(dst) || !m_device.IsValid(src)) {
        Violation("rhi.cmd.invalid_copy_handle", "유효하지 않은 버퍼로 복사를 시도했다");
        return;
    }
    if (m_inRenderPass) {
        Violation("rhi.cmd.copy_inside_pass", "렌더 패스 안에서 복사를 시도했다");
    }
    m_device.Stats().uploadedBytes += size;
}

void NullCommandList::CopyTexture(TextureHandle dst, TextureHandle src) {
    if (!m_device.IsValid(dst) || !m_device.IsValid(src)) {
        Violation("rhi.cmd.invalid_copy_handle", "유효하지 않은 텍스처로 복사를 시도했다");
    }
}

void NullCommandList::BeginGpuZone(const char* name) {
    ++m_gpuZoneDepth;
    m_gpuZoneStack.push_back(name ? name : "gpu");
}

void NullCommandList::EndGpuZone() {
    if (m_gpuZoneDepth == 0) {
        Violation("rhi.cmd.gpu_zone_underflow", "열리지 않은 GPU 존을 닫았다");
        return;
    }
    --m_gpuZoneDepth;
    const char* name = m_gpuZoneStack.back();
    m_gpuZoneStack.pop_back();
    // Null 백엔드에는 실제 GPU 시간이 없다. 0으로 보고하되 구간 구조는 유지한다.
    m_device.RecordGpuTiming(name, 0);
}

// ── 등록 ───────────────────────────────────────────────────────────────────
struct NullBackendRegistrar {
    NullBackendRegistrar() {
        RegisterBackend(Backend::Null, [](const DeviceCreateInfo& info)
                            -> Result<std::unique_ptr<IDevice>> {
            return std::unique_ptr<IDevice>(new NullDevice(info));
        });
    }
};

const NullBackendRegistrar g_nullRegistrar;

} // namespace

/// 정적 라이브러리에서 등록자가 링커에 잘려나가지 않게 붙잡아 두는 훅.
/// 이걸 어딘가에서 한 번 부르지 않으면 Null 백엔드가 조용히 사라진다 —
/// 정적 라이브러리 + 정적 초기화의 고전적인 함정이다.
void ForceLinkNullBackend() noexcept {
    ALICE_UNUSED(&g_nullRegistrar);
}

} // namespace alice::rhi
