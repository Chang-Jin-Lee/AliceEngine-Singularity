// SPDX-License-Identifier: MIT
#include "RHITypes.h"

#include "Foundation/StringUtil.h"

namespace alice::rhi {

const char* ToString(Format f) noexcept {
    switch (f) {
        case Format::Unknown:        return "unknown";
        case Format::R8Unorm:        return "r8_unorm";
        case Format::R8Snorm:        return "r8_snorm";
        case Format::R8Uint:         return "r8_uint";
        case Format::R8Sint:         return "r8_sint";
        case Format::RG8Unorm:       return "rg8_unorm";
        case Format::RG8Snorm:       return "rg8_snorm";
        case Format::RG8Uint:        return "rg8_uint";
        case Format::RG8Sint:        return "rg8_sint";
        case Format::RGBA8Unorm:     return "rgba8_unorm";
        case Format::RGBA8UnormSrgb: return "rgba8_unorm_srgb";
        case Format::RGBA8Snorm:     return "rgba8_snorm";
        case Format::RGBA8Uint:      return "rgba8_uint";
        case Format::RGBA8Sint:      return "rgba8_sint";
        case Format::BGRA8Unorm:     return "bgra8_unorm";
        case Format::BGRA8UnormSrgb: return "bgra8_unorm_srgb";
        case Format::R16Float:       return "r16_float";
        case Format::R16Uint:        return "r16_uint";
        case Format::R16Sint:        return "r16_sint";
        case Format::R16Unorm:       return "r16_unorm";
        case Format::RG16Float:      return "rg16_float";
        case Format::RG16Uint:       return "rg16_uint";
        case Format::RG16Sint:       return "rg16_sint";
        case Format::RG16Unorm:      return "rg16_unorm";
        case Format::RGBA16Float:    return "rgba16_float";
        case Format::RGBA16Uint:     return "rgba16_uint";
        case Format::RGBA16Sint:     return "rgba16_sint";
        case Format::RGBA16Unorm:    return "rgba16_unorm";
        case Format::R32Float:       return "r32_float";
        case Format::R32Uint:        return "r32_uint";
        case Format::R32Sint:        return "r32_sint";
        case Format::RG32Float:      return "rg32_float";
        case Format::RG32Uint:       return "rg32_uint";
        case Format::RG32Sint:       return "rg32_sint";
        case Format::RGB32Float:     return "rgb32_float";
        case Format::RGB32Uint:      return "rgb32_uint";
        case Format::RGB32Sint:      return "rgb32_sint";
        case Format::RGBA32Float:    return "rgba32_float";
        case Format::RGBA32Uint:     return "rgba32_uint";
        case Format::RGBA32Sint:     return "rgba32_sint";
        case Format::RGB10A2Unorm:   return "rgb10a2_unorm";
        case Format::RG11B10Float:   return "rg11b10_float";
        case Format::RGB9E5Float:    return "rgb9e5_float";
        case Format::D16Unorm:       return "d16_unorm";
        case Format::D24UnormS8Uint: return "d24_unorm_s8_uint";
        case Format::D32Float:       return "d32_float";
        case Format::D32FloatS8Uint: return "d32_float_s8_uint";
        case Format::BC1Unorm:       return "bc1_unorm";
        case Format::BC1UnormSrgb:   return "bc1_unorm_srgb";
        case Format::BC3Unorm:       return "bc3_unorm";
        case Format::BC3UnormSrgb:   return "bc3_unorm_srgb";
        case Format::BC4Unorm:       return "bc4_unorm";
        case Format::BC5Unorm:       return "bc5_unorm";
        case Format::BC6HFloat:      return "bc6h_float";
        case Format::BC7Unorm:       return "bc7_unorm";
        case Format::BC7UnormSrgb:   return "bc7_unorm_srgb";
        case Format::ETC2RGB8:       return "etc2_rgb8";
        case Format::ETC2RGBA8:      return "etc2_rgba8";
        case Format::ASTC4x4:        return "astc_4x4";
        case Format::ASTC6x6:        return "astc_6x6";
        case Format::ASTC8x8:        return "astc_8x8";
        case Format::Count:          return "count";
    }
    return "unknown";
}

u32 BytesPerPixel(Format f) noexcept {
    switch (f) {
        case Format::R8Unorm: case Format::R8Snorm:
        case Format::R8Uint:  case Format::R8Sint:
            return 1;

        case Format::RG8Unorm: case Format::RG8Snorm:
        case Format::RG8Uint:  case Format::RG8Sint:
        case Format::R16Float: case Format::R16Uint:
        case Format::R16Sint:  case Format::R16Unorm:
        case Format::D16Unorm:
            return 2;

        case Format::RGBA8Unorm: case Format::RGBA8UnormSrgb:
        case Format::RGBA8Snorm: case Format::RGBA8Uint: case Format::RGBA8Sint:
        case Format::BGRA8Unorm: case Format::BGRA8UnormSrgb:
        case Format::RG16Float:  case Format::RG16Uint:
        case Format::RG16Sint:   case Format::RG16Unorm:
        case Format::R32Float:   case Format::R32Uint: case Format::R32Sint:
        case Format::RGB10A2Unorm: case Format::RG11B10Float: case Format::RGB9E5Float:
        case Format::D24UnormS8Uint: case Format::D32Float:
            return 4;

        case Format::D32FloatS8Uint:
            return 5;

        case Format::RGBA16Float: case Format::RGBA16Uint:
        case Format::RGBA16Sint:  case Format::RGBA16Unorm:
        case Format::RG32Float:   case Format::RG32Uint: case Format::RG32Sint:
            return 8;

        case Format::RGB32Float: case Format::RGB32Uint: case Format::RGB32Sint:
            return 12;

        case Format::RGBA32Float: case Format::RGBA32Uint: case Format::RGBA32Sint:
            return 16;

        default:
            return 0;   // 압축 포맷과 Unknown
    }
}

bool IsDepthFormat(Format f) noexcept {
    return f == Format::D16Unorm || f == Format::D24UnormS8Uint ||
           f == Format::D32Float || f == Format::D32FloatS8Uint;
}

bool IsCompressed(Format f) noexcept {
    return f >= Format::BC1Unorm && f <= Format::ASTC8x8;
}

bool IsSrgb(Format f) noexcept {
    return f == Format::RGBA8UnormSrgb || f == Format::BGRA8UnormSrgb ||
           f == Format::BC1UnormSrgb || f == Format::BC3UnormSrgb ||
           f == Format::BC7UnormSrgb;
}

const char* ToString(ShaderStage s) noexcept {
    switch (s) {
        case ShaderStage::Vertex:         return "vertex";
        case ShaderStage::Hull:           return "hull";
        case ShaderStage::Domain:         return "domain";
        case ShaderStage::Geometry:       return "geometry";
        case ShaderStage::Pixel:          return "pixel";
        case ShaderStage::Compute:        return "compute";
        case ShaderStage::Amplification:  return "amplification";
        case ShaderStage::Mesh:           return "mesh";
        case ShaderStage::RayGen:         return "raygen";
        case ShaderStage::Miss:           return "miss";
        case ShaderStage::ClosestHit:     return "closesthit";
        case ShaderStage::AnyHit:         return "anyhit";
        case ShaderStage::Intersection:   return "intersection";
        case ShaderStage::Count:          return "count";
    }
    return "unknown";
}

std::string Capabilities::ToJson() const {
    std::string out = "{\"device\":";
    out += JsonQuote(deviceName);
    out += ",\"driver\":";
    out += JsonQuote(driverVersion);
    out += ",\"videoMemoryMB\":";
    detail::FormatAppend(out, videoMemoryBytes / (1024 * 1024));

    out += ",\"features\":{";
    out += "\"bindless\":";           out += bindlessResources ? "true" : "false";
    out += ",\"compute\":";           out += computeShaders ? "true" : "false";
    out += ",\"geometry\":";          out += geometryShaders ? "true" : "false";
    out += ",\"tessellation\":";      out += tessellation ? "true" : "false";
    out += ",\"meshShaders\":";       out += meshShaders ? "true" : "false";
    out += ",\"rayTracing\":";        out += rayTracing ? "true" : "false";
    out += ",\"vrs\":";               out += variableRateShading ? "true" : "false";
    out += ",\"conservativeRaster\":"; out += conservativeRaster ? "true" : "false";
    out += ",\"timestampQueries\":";  out += timestampQueries ? "true" : "false";
    out += ",\"pipelineStatistics\":"; out += pipelineStatistics ? "true" : "false";
    out += "}";

    out += ",\"limits\":{";
    out += "\"maxTexture2D\":";       detail::FormatAppend(out, maxTextureDimension2D);
    out += ",\"maxArrayLayers\":";    detail::FormatAppend(out, maxTextureArrayLayers);
    out += ",\"maxColorAttachments\":"; detail::FormatAppend(out, maxColorAttachments);
    out += ",\"maxAnisotropy\":";     detail::FormatAppend(out, maxAnisotropy);
    out += ",\"maxSampleCount\":";    detail::FormatAppend(out, maxSampleCount);
    out += ",\"maxComputeGroup\":[";
    detail::FormatAppend(out, maxComputeGroupSize[0]); out += ',';
    detail::FormatAppend(out, maxComputeGroupSize[1]); out += ',';
    detail::FormatAppend(out, maxComputeGroupSize[2]);
    out += "]}";

    out += '}';
    return out;
}

} // namespace alice::rhi
