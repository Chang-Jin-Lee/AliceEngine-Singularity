// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — RHI/RHITypes.h
//
// 백엔드 중립 그래픽 타입. DX11 / DX12 / Vulkan / Metal 이 전부 여기로 들어온다.
//
// 설계 기준은 "가장 낮은 공통분모"가 아니라 **"가장 현대적인 모델 + 하위 호환 에뮬레이션"**이다.
//   • 명령 목록(command list) 기반  — DX12/Vulkan/Metal 의 모델. DX11 은 즉시 컨텍스트로 흉내낸다
//   • 핸들 기반                    — 포인터가 아니라 32비트 핸들. 로그·직렬화·프로파일에 그대로 실린다
//   • 바인드리스 우선               — 지원하면 쓰고, 아니면 바인딩 테이블로 떨어진다
//
// 반대로 하면(DX11 을 기준으로 잡고 위로 확장) DX12/Vulkan 에서 성능을 못 낸다.
// 참고한 엔진 중 WickedEngine 과 Spartan 이 이 방향을 택했고, 실제로 잘 굴러간다.
//
// 왜 핸들인가:
//   포인터는 로그에 찍어도 의미가 없고, 프레임 간 비교가 안 되고, AI 가 읽을 수 없다.
//   "Buffer#42 가 매 프레임 재생성되고 있다" 는 문장이 나오려면 안정된 id 가 필요하다.
#pragma once

#include "Foundation/Core.h"

#include <string>
#include <vector>

namespace alice::rhi {

// ── 핸들 ───────────────────────────────────────────────────────────────────
/// 세대(generation)를 포함한 불투명 핸들. 해제된 핸들을 재사용해도 즉시 잡힌다.
template <typename Tag>
struct Handle {
    u32 index      = 0;
    u16 generation = 0;
    u16 unused     = 0;

    bool IsValid() const noexcept { return index != 0; }
    bool operator==(const Handle& o) const noexcept {
        return index == o.index && generation == o.generation;
    }
    bool operator!=(const Handle& o) const noexcept { return !(*this == o); }

    /// 로그·진단용 표현. "Buffer#42:3"
    u64 Key() const noexcept { return (static_cast<u64>(generation) << 32) | index; }
};

struct BufferTag;
struct TextureTag;
struct SamplerTag;
struct ShaderTag;
struct PipelineTag;
struct RenderPassTag;
struct SwapchainTag;

using BufferHandle     = Handle<BufferTag>;
using TextureHandle    = Handle<TextureTag>;
using SamplerHandle    = Handle<SamplerTag>;
using ShaderHandle     = Handle<ShaderTag>;
using PipelineHandle   = Handle<PipelineTag>;
using RenderPassHandle = Handle<RenderPassTag>;
using SwapchainHandle  = Handle<SwapchainTag>;

// ── 포맷 ───────────────────────────────────────────────────────────────────
enum class Format : u8 {
    Unknown = 0,

    R8Unorm, R8Snorm, R8Uint, R8Sint,
    RG8Unorm, RG8Snorm, RG8Uint, RG8Sint,
    RGBA8Unorm, RGBA8UnormSrgb, RGBA8Snorm, RGBA8Uint, RGBA8Sint,
    BGRA8Unorm, BGRA8UnormSrgb,

    R16Float, R16Uint, R16Sint, R16Unorm,
    RG16Float, RG16Uint, RG16Sint, RG16Unorm,
    RGBA16Float, RGBA16Uint, RGBA16Sint, RGBA16Unorm,

    R32Float, R32Uint, R32Sint,
    RG32Float, RG32Uint, RG32Sint,
    RGB32Float, RGB32Uint, RGB32Sint,
    RGBA32Float, RGBA32Uint, RGBA32Sint,

    RGB10A2Unorm, RG11B10Float, RGB9E5Float,

    D16Unorm, D24UnormS8Uint, D32Float, D32FloatS8Uint,

    // 압축 — 데스크톱
    BC1Unorm, BC1UnormSrgb, BC3Unorm, BC3UnormSrgb,
    BC4Unorm, BC5Unorm, BC6HFloat, BC7Unorm, BC7UnormSrgb,
    // 압축 — 모바일
    ETC2RGB8, ETC2RGBA8, ASTC4x4, ASTC6x6, ASTC8x8,

    Count
};

const char* ToString(Format f) noexcept;
u32  BytesPerPixel(Format f) noexcept;    ///< 압축 포맷은 0 을 돌려준다
bool IsDepthFormat(Format f) noexcept;
bool IsCompressed(Format f) noexcept;
bool IsSrgb(Format f) noexcept;

// ── 열거형들 ───────────────────────────────────────────────────────────────
enum class PrimitiveTopology : u8 {
    PointList, LineList, LineStrip, TriangleList, TriangleStrip, PatchList,
};

enum class CompareOp : u8 {
    Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always,
};

enum class BlendFactor : u8 {
    Zero, One,
    SrcColor, InvSrcColor, SrcAlpha, InvSrcAlpha,
    DstColor, InvDstColor, DstAlpha, InvDstAlpha,
    SrcAlphaSaturate, ConstantColor, InvConstantColor,
};

enum class BlendOp : u8 { Add, Subtract, ReverseSubtract, Min, Max };

enum class CullMode : u8 { None, Front, Back };
enum class FillMode : u8 { Solid, Wireframe };
enum class FrontFace : u8 { CounterClockwise, Clockwise };

enum class FilterMode : u8 { Point, Linear, Anisotropic };
enum class AddressMode : u8 { Repeat, MirrorRepeat, ClampToEdge, ClampToBorder };

enum class ShaderStage : u8 {
    Vertex, Hull, Domain, Geometry, Pixel, Compute, Amplification, Mesh,
    RayGen, Miss, ClosestHit, AnyHit, Intersection,
    Count
};

const char* ToString(ShaderStage s) noexcept;

/// 리소스 사용 의도. 백엔드가 알아서 상태 전이를 만든다.
/// 명시적 배리어를 콘텐츠 레벨에 노출하지 않는 것이 이 엔진의 방침이다 —
/// 사람도 AI 도 배리어를 정확히 쓰지 못한다. RenderGraph 가 자동으로 계산한다.
enum class ResourceUsage : u32 {
    None            = 0,
    VertexBuffer    = 1u << 0,
    IndexBuffer     = 1u << 1,
    ConstantBuffer  = 1u << 2,
    ShaderResource  = 1u << 3,
    UnorderedAccess = 1u << 4,
    RenderTarget    = 1u << 5,
    DepthStencil    = 1u << 6,
    CopySource      = 1u << 7,
    CopyDest        = 1u << 8,
    IndirectArgs    = 1u << 9,
    Present         = 1u << 10,
};

inline ResourceUsage operator|(ResourceUsage a, ResourceUsage b) noexcept {
    return static_cast<ResourceUsage>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline bool HasFlag(ResourceUsage value, ResourceUsage flag) noexcept {
    return (static_cast<u32>(value) & static_cast<u32>(flag)) != 0;
}

/// CPU 접근 방식. 어디에 놓을지는 백엔드가 정한다.
enum class MemoryAccess : u8 {
    GpuOnly,       ///< 기본. 가장 빠르다
    CpuToGpu,      ///< 업로드용. 매 프레임 쓰는 상수 버퍼
    GpuToCpu,      ///< 리드백. 프로파일링·스크린샷
};

enum class LoadOp : u8 { Load, Clear, DontCare };
enum class StoreOp : u8 { Store, DontCare };

// ── 기술자 구조체 ──────────────────────────────────────────────────────────
struct BufferDesc {
    u64           size = 0;
    u32           stride = 0;          ///< 구조화 버퍼일 때 요소 크기
    ResourceUsage usage = ResourceUsage::None;
    MemoryAccess  access = MemoryAccess::GpuOnly;
    std::string   debugName;           ///< 프로파일러와 로그에 그대로 나온다. 비우지 말 것
};

struct TextureDesc {
    u32           width = 1;
    u32           height = 1;
    u32           depth = 1;
    u32           mipLevels = 1;
    u32           arraySize = 1;
    u32           sampleCount = 1;
    Format        format = Format::RGBA8Unorm;
    ResourceUsage usage = ResourceUsage::ShaderResource;
    bool          isCube = false;
    std::string   debugName;
};

struct SamplerDesc {
    FilterMode  minFilter = FilterMode::Linear;
    FilterMode  magFilter = FilterMode::Linear;
    FilterMode  mipFilter = FilterMode::Linear;
    AddressMode addressU = AddressMode::Repeat;
    AddressMode addressV = AddressMode::Repeat;
    AddressMode addressW = AddressMode::Repeat;
    u32         maxAnisotropy = 1;
    CompareOp   compareOp = CompareOp::Never;
    bool        comparisonSampler = false;
    f32         mipBias = 0.0f;
    std::string debugName;
};

struct ShaderDesc {
    ShaderStage      stage = ShaderStage::Vertex;
    std::string      entryPoint = "main";
    std::string      debugName;
    /// 컴파일된 바이트코드. 백엔드에 맞는 형식(DXIL / SPIR-V / MSL)이어야 한다.
    const void*      bytecode = nullptr;
    usize            bytecodeSize = 0;
};

struct BlendState {
    bool        enabled = false;
    BlendFactor srcColor = BlendFactor::One;
    BlendFactor dstColor = BlendFactor::Zero;
    BlendOp     colorOp = BlendOp::Add;
    BlendFactor srcAlpha = BlendFactor::One;
    BlendFactor dstAlpha = BlendFactor::Zero;
    BlendOp     alphaOp = BlendOp::Add;
    u8          writeMask = 0xF;
};

struct DepthStencilState {
    bool      depthTest = true;
    bool      depthWrite = true;
    CompareOp depthCompare = CompareOp::LessEqual;
    bool      stencilEnabled = false;
};

struct RasterizerState {
    FillMode  fillMode = FillMode::Solid;
    CullMode  cullMode = CullMode::Back;
    FrontFace frontFace = FrontFace::CounterClockwise;
    i32       depthBias = 0;
    f32       slopeScaledDepthBias = 0.0f;
    bool      depthClipEnable = true;
    bool      conservativeRaster = false;
};

struct VertexAttribute {
    std::string semantic;        ///< "POSITION", "NORMAL", "TEXCOORD0"
    u32         location = 0;
    u32         bufferSlot = 0;
    u32         offset = 0;
    Format      format = Format::RGB32Float;
};

struct GraphicsPipelineDesc {
    ShaderHandle                 vertexShader;
    ShaderHandle                 pixelShader;
    std::vector<VertexAttribute> vertexLayout;
    PrimitiveTopology            topology = PrimitiveTopology::TriangleList;
    RasterizerState              rasterizer;
    DepthStencilState            depthStencil;
    BlendState                   blend[8];
    u32                          renderTargetCount = 1;
    Format                       renderTargetFormats[8] = {Format::RGBA8Unorm};
    Format                       depthFormat = Format::D32Float;
    u32                          sampleCount = 1;
    std::string                  debugName;
};

struct ComputePipelineDesc {
    ShaderHandle computeShader;
    std::string  debugName;
};

struct SwapchainDesc {
    void*  windowHandle = nullptr;   ///< HWND / NSWindow* / ANativeWindow*
    u32    width = 0;
    u32    height = 0;
    Format format = Format::BGRA8Unorm;
    u32    bufferCount = 3;
    bool   vsync = true;
    bool   allowTearing = false;
    std::string debugName;
};

struct Viewport {
    f32 x = 0.0f, y = 0.0f;
    f32 width = 0.0f, height = 0.0f;
    f32 minDepth = 0.0f, maxDepth = 1.0f;
};

struct ScissorRect {
    i32 x = 0, y = 0;
    i32 width = 0, height = 0;
};

struct ClearValue {
    f32 color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    f32 depth = 1.0f;
    u8  stencil = 0;
};

struct RenderTargetBinding {
    TextureHandle texture;
    u32           mipLevel = 0;
    u32           arraySlice = 0;
    LoadOp        loadOp = LoadOp::Clear;
    StoreOp       storeOp = StoreOp::Store;
    ClearValue    clearValue;
};

struct RenderPassDesc {
    RenderTargetBinding colorTargets[8];
    u32                 colorTargetCount = 0;
    RenderTargetBinding depthTarget;
    bool                hasDepth = false;
    std::string         debugName;
};

// ── 능력 질의 ──────────────────────────────────────────────────────────────
/// 백엔드가 무엇을 할 수 있는지. **콘텐츠는 이 값을 보고 갈라진다.**
/// #ifdef 로 플랫폼을 가르지 않는 이유: 콘텐츠는 텍스트 문서이고, 문서에는 전처리기가 없다.
struct Capabilities {
    std::string deviceName;
    std::string driverVersion;
    u64         videoMemoryBytes = 0;

    bool bindlessResources = false;
    bool computeShaders    = true;
    bool geometryShaders   = false;
    bool tessellation      = false;
    bool meshShaders       = false;
    bool rayTracing        = false;
    bool variableRateShading = false;
    bool conservativeRaster = false;
    bool timestampQueries  = true;
    bool pipelineStatistics = false;

    u32 maxTextureDimension2D = 16384;
    u32 maxTextureArrayLayers = 2048;
    u32 maxColorAttachments   = 8;
    u32 maxComputeGroupSize[3] = {1024, 1024, 64};
    u32 maxAnisotropy         = 16;
    u32 maxSampleCount        = 8;

    /// 이 정보를 그대로 로그와 진단에 싣는다. "왜 이 기능이 안 되는가"의 답이 여기 있다.
    std::string ToJson() const;
};

} // namespace alice::rhi
