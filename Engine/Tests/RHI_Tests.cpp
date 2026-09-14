// SPDX-License-Identifier: MIT
//
// Null 백엔드는 검증기를 겸한다. 그래서 이 테스트들은 "그리기가 되는가"가 아니라
// **"잘못된 사용을 잡아내는가"**를 본다. 여기서 통과하지 못하는 코드는 진짜 GPU 에서도
// 통과하지 못한다 — 다만 진짜 GPU 는 조용히 깨지거나 드라이버가 죽을 뿐이다.
#include "TestFramework.h"

#include "Doc/JsonParser.h"
#include "Foundation/Log.h"
#include "RHI/RHIBackend.h"

using namespace alice;
using namespace alice::rhi;

namespace {

/// 로그를 잡아 특정 이벤트가 나왔는지 본다. Null 백엔드는 위반을 로그로 보고한다.
class EventCapture : public ILogSink {
public:
    void Write(const LogRecord& record) override { events.emplace_back(record.event); }
    bool Saw(std::string_view event) const {
        for (const std::string& e : events) {
            if (e == event) return true;
        }
        return false;
    }
    std::vector<std::string> events;
};

struct CaptureScope {
    std::shared_ptr<EventCapture> sink = std::make_shared<EventCapture>();
    LogLevel previousLevel = Log::GetLevel();

    CaptureScope() {
        Log::RemoveAllSinks();
        Log::AddSink(sink);
        Log::SetLevel(LogLevel::Trace);
    }
    ~CaptureScope() {
        Log::RemoveAllSinks();
        Log::SetLevel(previousLevel);
    }
};

std::unique_ptr<IDevice> MakeNullDevice() {
    LinkAllBackends();
    DeviceCreateInfo info;
    info.backend          = Backend::Null;
    info.enableValidation = true;
    Result<std::unique_ptr<IDevice>> device = CreateDevice(info);
    return device.IsOk() ? std::move(device.Value()) : nullptr;
}

BufferDesc SimpleBuffer(const char* name, u64 size = 256) {
    BufferDesc desc;
    desc.size      = size;
    desc.usage     = ResourceUsage::VertexBuffer;
    desc.debugName = name;
    return desc;
}

TextureDesc SimpleTexture(const char* name) {
    TextureDesc desc;
    desc.width     = 64;
    desc.height    = 64;
    desc.usage     = ResourceUsage::RenderTarget;
    desc.debugName = name;
    return desc;
}

} // namespace

// ── 백엔드 선택 ────────────────────────────────────────────────────────────
ALICE_TEST(RHI, NullBackendIsAlwaysAvailable) {
    LinkAllBackends();
    ALICE_CHECK_MSG(IsBackendAvailable(Backend::Null),
                    "Null 백엔드는 항상 있어야 한다. 없으면 CI 가 렌더 경로를 못 돈다");
}

ALICE_TEST(RHI, PreferredOrderEndsWithNull) {
    const std::vector<Backend> order = PreferredBackendOrder();
    ALICE_REQUIRE(!order.empty());
    ALICE_CHECK_MSG(order.back() == Backend::Null,
                    "자동 선택은 마지막에 Null 로 떨어져야 한다. 엔진이 못 뜨는 상황을 없애기 위해서다");
}

ALICE_TEST(RHI, BackendNameRoundTrip) {
    for (const char* name : {"null", "d3d11", "d3d12", "vulkan", "metal"}) {
        Backend b = Backend::Count;
        ALICE_CHECK_MSG(ParseBackend(name, b), alice::Fmt("'{}' 를 해석하지 못했다", name));
        ALICE_CHECK_STR(ToString(b), name);
    }
}

ALICE_TEST(RHI, MissingBackendGivesActionableError) {
    LinkAllBackends();
    if (IsBackendAvailable(Backend::Metal)) return;   // 맥에서는 건너뛴다

    DeviceCreateInfo info;
    info.backend = Backend::Metal;
    Result<std::unique_ptr<IDevice>> device = CreateDevice(info);

    ALICE_REQUIRE(device.IsErr());
    ALICE_CHECK_STR(device.Error().code, "rhi.backend_unavailable");
    ALICE_CHECK_MSG(device.Error().hint.find("ALICE_RHI_") != std::string::npos,
                    "빠진 백엔드 에러에는 어느 CMake 옵션을 봐야 하는지 나와야 한다");
}

// ── 리소스 수명 ────────────────────────────────────────────────────────────
ALICE_TEST(RHI, HandlesAreInvalidatedOnDestroy) {
    std::unique_ptr<IDevice> device = MakeNullDevice();
    ALICE_REQUIRE(device != nullptr);

    Result<BufferHandle> buffer = device->CreateBuffer(SimpleBuffer("test"));
    ALICE_REQUIRE(buffer.IsOk());

    const BufferHandle stale = *buffer;
    device->Destroy(stale);

    // 해제된 핸들로 매핑하면 즉시 잡혀야 한다. 진짜 백엔드에서는 이게 크래시다.
    Result<void*> mapped = device->MapBuffer(stale);
    ALICE_CHECK(mapped.IsErr());
    ALICE_CHECK_STR(mapped.Error().code, "rhi.invalid_handle");

    device->Destroy(SwapchainHandle{});   // 무효 핸들 파괴도 죽지 않아야 한다
}

ALICE_TEST(RHI, LeakIsReportedOnShutdown) {
    CaptureScope capture;
    {
        std::unique_ptr<IDevice> device = MakeNullDevice();
        ALICE_REQUIRE(device != nullptr);
        Result<BufferHandle> leaked = device->CreateBuffer(SimpleBuffer("leaked-buffer"));
        ALICE_REQUIRE(leaked.IsOk());
        // 일부러 해제하지 않는다
    }
    ALICE_CHECK_MSG(capture.sink->Saw("rhi.leak.detected"),
                    "종료 시점에 살아 있는 리소스는 반드시 보고되어야 한다");
    ALICE_CHECK_MSG(capture.sink->Saw("rhi.leak.buffer"),
                    "누수 보고에는 어느 리소스인지 이름이 함께 나와야 한다");
}

ALICE_TEST(RHI, GpuOnlyBufferCannotBeMapped) {
    std::unique_ptr<IDevice> device = MakeNullDevice();
    ALICE_REQUIRE(device != nullptr);

    BufferDesc desc = SimpleBuffer("gpu-only");
    desc.access = MemoryAccess::GpuOnly;
    Result<BufferHandle> buffer = device->CreateBuffer(desc);
    ALICE_REQUIRE(buffer.IsOk());

    Result<void*> mapped = device->MapBuffer(*buffer);
    ALICE_REQUIRE(mapped.IsErr());
    ALICE_CHECK_STR(mapped.Error().code, "rhi.buffer_not_mappable");
    ALICE_CHECK_MSG(mapped.Error().hint.find("UploadBuffer") != std::string::npos,
                    "대안(UploadBuffer)을 알려줘야 한다");

    device->Destroy(*buffer);
}

ALICE_TEST(RHI, EmptyShaderBytecodeIsRejected) {
    std::unique_ptr<IDevice> device = MakeNullDevice();
    ALICE_REQUIRE(device != nullptr);

    ShaderDesc desc;
    desc.debugName    = "empty.vs";
    desc.bytecodeSize = 0;

    Result<ShaderHandle> shader = device->CreateShader(desc);
    ALICE_REQUIRE(shader.IsErr());
    ALICE_CHECK_STR(shader.Error().code, "rhi.empty_shader");
}

ALICE_TEST(RHI, UploadBeyondBufferEndIsRejected) {
    std::unique_ptr<IDevice> device = MakeNullDevice();
    ALICE_REQUIRE(device != nullptr);

    BufferDesc desc = SimpleBuffer("small", 16);
    desc.access = MemoryAccess::CpuToGpu;
    Result<BufferHandle> buffer = device->CreateBuffer(desc);
    ALICE_REQUIRE(buffer.IsOk());

    const u8 data[32] = {};
    Status status = device->UploadBuffer(*buffer, data, sizeof(data));
    ALICE_REQUIRE(status.IsErr());
    ALICE_CHECK_STR(status.Error().code, "rhi.upload_out_of_range");

    device->Destroy(*buffer);
}

// ── 명령 목록 계약 ─────────────────────────────────────────────────────────
ALICE_TEST(RHI, DrawOutsideRenderPassIsCaught) {
    CaptureScope capture;
    std::unique_ptr<IDevice> device = MakeNullDevice();
    ALICE_REQUIRE(device != nullptr);

    ICommandList* list = device->AcquireCommandList("test");
    ALICE_REQUIRE(list != nullptr);
    list->Begin();
    list->Draw(3);
    list->End();

    ALICE_CHECK(capture.sink->Saw("rhi.cmd.draw_outside_pass"));
}

ALICE_TEST(RHI, UnclosedRenderPassIsCaught) {
    CaptureScope capture;
    std::unique_ptr<IDevice> device = MakeNullDevice();
    ALICE_REQUIRE(device != nullptr);

    Result<TextureHandle> target = device->CreateTexture(SimpleTexture("rt"));
    ALICE_REQUIRE(target.IsOk());

    RenderPassDesc pass;
    pass.colorTargetCount     = 1;
    pass.colorTargets[0].texture = *target;
    pass.debugName            = "main";

    ICommandList* list = device->AcquireCommandList("test");
    list->Begin();
    list->BeginRenderPass(pass);
    list->End();   // EndRenderPass 를 빼먹었다

    ALICE_CHECK(capture.sink->Saw("rhi.cmd.unclosed_render_pass"));
    device->Destroy(*target);
}

ALICE_TEST(RHI, UnbalancedDebugGroupIsCaught) {
    CaptureScope capture;
    std::unique_ptr<IDevice> device = MakeNullDevice();
    ALICE_REQUIRE(device != nullptr);

    ICommandList* list = device->AcquireCommandList("test");
    list->Begin();
    list->PushDebugGroup("shadow");
    list->End();

    ALICE_CHECK(capture.sink->Saw("rhi.cmd.unbalanced_debug_group"));
}

ALICE_TEST(RHI, ScopedGpuZoneStaysBalanced) {
    CaptureScope capture;
    std::unique_ptr<IDevice> device = MakeNullDevice();
    ALICE_REQUIRE(device != nullptr);

    ICommandList* list = device->AcquireCommandList("test");
    list->Begin();
    {
        ALICE_GPU_ZONE(*list, "Shadow");
        {
            ALICE_GPU_ZONE(*list, "Cascade0");
        }
    }
    list->End();

    ALICE_CHECK_MSG(!capture.sink->Saw("rhi.cmd.unbalanced_gpu_zone"),
                    "스코프 존은 항상 균형이 맞아야 한다");
    ALICE_CHECK_MSG(!capture.sink->Saw("rhi.cmd.unbalanced_debug_group"), "");

    u64 frame = 0;
    const std::vector<GpuTimingResult> timings = device->CollectGpuTimings(frame);
    ALICE_CHECK(timings.size() == 2);
}

ALICE_TEST(RHI, PushConstantsOverPortableLimitIsCaught) {
    CaptureScope capture;
    std::unique_ptr<IDevice> device = MakeNullDevice();
    ALICE_REQUIRE(device != nullptr);

    ICommandList* list = device->AcquireCommandList("test");
    list->Begin();
    const u8 tooMuch[256] = {};
    list->SetPushConstants(ShaderStage::Vertex, tooMuch, sizeof(tooMuch));
    list->End();

    ALICE_CHECK_MSG(capture.sink->Saw("rhi.cmd.push_constants_too_large"),
                    "Vulkan 이 보장하는 128바이트를 넘으면 이식성이 깨진다. 미리 잡아야 한다");
}

ALICE_TEST(RHI, BadIndexBufferFormatIsCaught) {
    CaptureScope capture;
    std::unique_ptr<IDevice> device = MakeNullDevice();
    ALICE_REQUIRE(device != nullptr);

    Result<BufferHandle> buffer = device->CreateBuffer(SimpleBuffer("ib"));
    ALICE_REQUIRE(buffer.IsOk());

    ICommandList* list = device->AcquireCommandList("test");
    list->Begin();
    list->SetIndexBuffer(*buffer, Format::RGBA8Unorm, 0);
    list->End();

    ALICE_CHECK(capture.sink->Saw("rhi.cmd.bad_index_format"));
    device->Destroy(*buffer);
}

ALICE_TEST(RHI, FrameBracketingIsEnforced) {
    std::unique_ptr<IDevice> device = MakeNullDevice();
    ALICE_REQUIRE(device != nullptr);

    ALICE_CHECK(device->EndFrame().IsErr());        // BeginFrame 없이
    ALICE_CHECK(device->BeginFrame(1).IsOk());
    ALICE_CHECK(device->BeginFrame(2).IsErr());     // 중복 Begin
    ALICE_CHECK(device->EndFrame().IsOk());
}

// ── 포맷 유틸 ──────────────────────────────────────────────────────────────
ALICE_TEST(RHIFormat, ByteSizes) {
    ALICE_CHECK(BytesPerPixel(Format::R8Unorm) == 1);
    ALICE_CHECK(BytesPerPixel(Format::RGBA8Unorm) == 4);
    ALICE_CHECK(BytesPerPixel(Format::RGBA16Float) == 8);
    ALICE_CHECK(BytesPerPixel(Format::RGBA32Float) == 16);
    ALICE_CHECK(BytesPerPixel(Format::BC7Unorm) == 0);   // 압축은 픽셀당 크기가 없다
}

ALICE_TEST(RHIFormat, Classification) {
    ALICE_CHECK(IsDepthFormat(Format::D32Float));
    ALICE_CHECK(!IsDepthFormat(Format::RGBA8Unorm));
    ALICE_CHECK(IsCompressed(Format::BC7Unorm));
    ALICE_CHECK(IsCompressed(Format::ASTC4x4));
    ALICE_CHECK(!IsCompressed(Format::RGBA8Unorm));
    ALICE_CHECK(IsSrgb(Format::RGBA8UnormSrgb));
    ALICE_CHECK(!IsSrgb(Format::RGBA8Unorm));
}

ALICE_TEST(RHIFormat, EveryFormatHasAName) {
    // Unknown(0) 은 이름이 "unknown" 인 것이 맞다. 1부터 검사한다.
    for (u8 i = 1; i < static_cast<u8>(Format::Count); ++i) {
        const char* name = ToString(static_cast<Format>(i));
        ALICE_CHECK_MSG(name && name[0] != '\0' && std::string(name) != "unknown",
                        alice::Fmt("포맷 {} 에 이름이 없다", static_cast<i32>(i)));
    }
}

ALICE_TEST(RHI, CapabilitiesAreMachineReadable) {
    std::unique_ptr<IDevice> device = MakeNullDevice();
    ALICE_REQUIRE(device != nullptr);

    const std::string json = device->GetCapabilities().ToJson();
    DiagnosticBag bag;
    doc::Value parsed;
    ALICE_CHECK_MSG(doc::ParseJson(json, parsed, bag),
                    "능력 정보는 파싱 가능한 JSON 이어야 한다:\n" + bag.ToPretty());
    ALICE_CHECK(parsed["features"].Has("bindless"));
    ALICE_CHECK(parsed["limits"].Has("maxTexture2D"));
}
