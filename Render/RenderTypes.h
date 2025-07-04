#pragma once

#include "RenderPtr.h"

#include <assert.h>
#include <cstdint>

#include <memory>
#include <string>
#include <vector>

#include <wrl.h>

#define RENDER_TYPE(t) enum class t : uint32_t {INVALID}
#define FWD_RENDER_TYPE(t) enum class t : uint32_t

namespace rl
{

FWD_RENDER_TYPE(ShaderResourceView_t);
FWD_RENDER_TYPE(UnorderedAccessView_t);
FWD_RENDER_TYPE(RenderTargetView_t);
FWD_RENDER_TYPE(DepthStencilView_t);
FWD_RENDER_TYPE(VertexBuffer_t);
FWD_RENDER_TYPE(IndexBuffer_t);
FWD_RENDER_TYPE(StructuredBuffer_t);
FWD_RENDER_TYPE(ConstantBuffer_t);
FWD_RENDER_TYPE(DynamicBuffer_t);
FWD_RENDER_TYPE(GraphicsPipelineState_t);
FWD_RENDER_TYPE(ComputePipelineState_t);
FWD_RENDER_TYPE(RootSignature_t);
FWD_RENDER_TYPE(VertexShader_t);
FWD_RENDER_TYPE(PixelShader_t);
FWD_RENDER_TYPE(GeometryShader_t);
FWD_RENDER_TYPE(MeshShader_t);
FWD_RENDER_TYPE(AmplificationShader_t);
FWD_RENDER_TYPE(ComputeShader_t);
FWD_RENDER_TYPE(Texture_t);
FWD_RENDER_TYPE(IndirectCommand_t);
FWD_RENDER_TYPE(RaytracingGeometry_t);
FWD_RENDER_TYPE(RaytracingScene_t);
FWD_RENDER_TYPE(RaytracingPipelineState_t);
FWD_RENDER_TYPE(RaytracingShaderTable_t);


using ShaderResourceViewPtr = RenderPtr<ShaderResourceView_t>;
using UnorderedAccessViewPtr = RenderPtr<UnorderedAccessView_t>;
using RenderTargetViewPtr = RenderPtr<RenderTargetView_t>;
using DepthStencilViewPtr = RenderPtr<DepthStencilView_t>;
using VertexBufferPtr = RenderPtr<VertexBuffer_t>;
using IndexBufferPtr = RenderPtr<IndexBuffer_t>;
using StructuredBufferPtr = RenderPtr<StructuredBuffer_t>;
using ConstantBufferPtr = RenderPtr<ConstantBuffer_t>;
using GraphicsPipelineStatePtr = RenderPtr<GraphicsPipelineState_t>;
using ComputePipelineStatePtr = RenderPtr<ComputePipelineState_t>;
using RootSignaturePtr = RenderPtr<RootSignature_t>;
using TexturePtr = RenderPtr<Texture_t>;
using RaytracingGeometryPtr = RenderPtr<RaytracingGeometry_t>;
using RaytracingScenePtr = RenderPtr<RaytracingScene_t>;
using RaytracingPipelineStatePtr = RenderPtr<RaytracingPipelineState_t>;
using RaytracingShaderTablePtr = RenderPtr<RaytracingShaderTable_t>;

struct CommandList;
struct CommandListSubmissionGroup;
struct GraphicsPipelineTargetDesc;
struct ShaderMacro;
struct RenderView;
struct RenderInitParams;

using ShaderMacros = std::vector<ShaderMacro>;

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

#define IMPLEMENT_FLAGS(e, underlyingType) \
constexpr inline e operator&(e lhs, e rhs) noexcept {return (e)((underlyingType)lhs & (underlyingType)rhs); } \
constexpr inline e operator|(e lhs, e rhs) noexcept {return (e)((underlyingType)lhs | (underlyingType)rhs); } \
inline e& operator|=(e& lhs, e rhs) noexcept {return lhs = (e)((underlyingType)lhs | (underlyingType)rhs); } \

template<typename EnumT, EnumT TInvalidVal = EnumT(0)>
constexpr bool HasEnumFlags(EnumT flags, EnumT value)
{
    return (flags & value) != TInvalidVal;
}

template<typename EnumT>
constexpr EnumT AddEnumFlags(EnumT flags, EnumT value)
{
    return flags | value;
}

enum class RenderDebugWarnings : uint32_t
{
    RENDER_TARGET_NOT_SET,
};

enum class ShaderVisibility : uint32_t
{
    ALL,
    VERTEX,
    GEOMETRY,
    PIXEL,
    AMPLIFICATION,
    MESH,
};

enum class RenderFormat : uint32_t
{
    UNKNOWN = 0,
    R32G32B32A32_TYPELESS = 1,
    R32G32B32A32_FLOAT = 2,
    R32G32B32A32_UINT = 3,
    R32G32B32A32_SINT = 4,
    R32G32B32_TYPELESS = 5,
    R32G32B32_FLOAT = 6,
    R32G32B32_UINT = 7,
    R32G32B32_SINT = 8,
    R16G16B16A16_TYPELESS = 9,
    R16G16B16A16_FLOAT = 10,
    R16G16B16A16_UNORM = 11,
    R16G16B16A16_UINT = 12,
    R16G16B16A16_SNORM = 13,
    R16G16B16A16_SINT = 14,
    R32G32_TYPELESS = 15,
    R32G32_FLOAT = 16,
    R32G32_UINT = 17,
    R32G32_SINT = 18,
    R32G8X24_TYPELESS = 19,
    D32_FLOAT_S8X24_UINT = 20,
    R32_FLOAT_X8X24_TYPELESS = 21,
    X32_TYPELESS_G8X24_UINT = 22,
    R10G10B10A2_TYPELESS = 23,
    R10G10B10A2_UNORM = 24,
    R10G10B10A2_UINT = 25,
    R11G11B10_FLOAT = 26,
    R8G8B8A8_TYPELESS = 27,
    R8G8B8A8_UNORM = 28,
    R8G8B8A8_UNORM_SRGB = 29,
    R8G8B8A8_UINT = 30,
    R8G8B8A8_SNORM = 31,
    R8G8B8A8_SINT = 32,
    R16G16_TYPELESS = 33,
    R16G16_FLOAT = 34,
    R16G16_UNORM = 35,
    R16G16_UINT = 36,
    R16G16_SNORM = 37,
    R16G16_SINT = 38,
    R32_TYPELESS = 39,
    D32_FLOAT = 40,
    R32_FLOAT = 41,
    R32_UINT = 42,
    R32_SINT = 43,
    R24G8_TYPELESS = 44,
    D24_UNORM_S8_UINT = 45,
    R24_UNORM_X8_TYPELESS = 46,
    X24_TYPELESS_G8_UINT = 47,
    R8G8_TYPELESS = 48,
    R8G8_UNORM = 49,
    R8G8_UINT = 50,
    R8G8_SNORM = 51,
    R8G8_SINT = 52,
    R16_TYPELESS = 53,
    R16_FLOAT = 54,
    D16_UNORM = 55,
    R16_UNORM = 56,
    R16_UINT = 57,
    R16_SNORM = 58,
    R16_SINT = 59,
    R8_TYPELESS = 60,
    R8_UNORM = 61,
    R8_UINT = 62,
    R8_SNORM = 63,
    R8_SINT = 64,
    A8_UNORM = 65,
    R1_UNORM = 66,
    R9G9B9E5_SHAREDEXP = 67,
    R8G8_B8G8_UNORM = 68,
    G8R8_G8B8_UNORM = 69,
    BC1_TYPELESS = 70,
    BC1_UNORM = 71,
    BC1_UNORM_SRGB = 72,
    BC2_TYPELESS = 73,
    BC2_UNORM = 74,
    BC2_UNORM_SRGB = 75,
    BC3_TYPELESS = 76,
    BC3_UNORM = 77,
    BC3_UNORM_SRGB = 78,
    BC4_TYPELESS = 79,
    BC4_UNORM = 80,
    BC4_SNORM = 81,
    BC5_TYPELESS = 82,
    BC5_UNORM = 83,
    BC5_SNORM = 84,
    B5G6R5_UNORM = 85,
    B5G5R5A1_UNORM = 86,
    B8G8R8A8_UNORM = 87,
    B8G8R8X8_UNORM = 88,
    R10G10B10_XR_BIAS_A2_UNORM = 89,
    B8G8R8A8_TYPELESS = 90,
    B8G8R8A8_UNORM_SRGB = 91,
    B8G8R8X8_TYPELESS = 92,
    B8G8R8X8_UNORM_SRGB = 93,
    BC6H_TYPELESS = 94,
    BC6H_UF16 = 95,
    BC6H_SF16 = 96,
    BC7_TYPELESS = 97,
    BC7_UNORM = 98,
    BC7_UNORM_SRGB = 99,
    AYUV = 100,
    Y410 = 101,
    Y416 = 102,
    NV12 = 103,
    P010 = 104,
    P016 = 105,
    OPAQUE_420 = 106,
    YUY2 = 107,
    Y210 = 108,
    Y216 = 109,
    NV11 = 110,
    AI44 = 111,
    IA44 = 112,
    P8 = 113,
    A8P8 = 114,
    B4G4R4A4_UNORM = 115,
    COUNT,
    BIT_MASK = 0x7f,
};

enum class InputClassification : uint32_t
{
	PER_VERTEX,
	PER_INSTANCE,
};

struct InputElementDesc
{
	const char* semanticName;
	uint32_t semanticIndex;
	RenderFormat format;
	uint32_t inputSlot;
	uint32_t alignedByteOffset;
	InputClassification inputSlotClass;
	uint32_t instanceDataStepRate;

    bool operator==(InputElementDesc& other) const
    {
        return strcmp(semanticName, other.semanticName) == 0 &&
            semanticIndex == other.semanticIndex &&
            format == other.format &&
            inputSlot == other.inputSlot &&
            alignedByteOffset == other.alignedByteOffset &&
            inputSlotClass == other.inputSlotClass &&
            instanceDataStepRate == other.instanceDataStepRate;
    }
};

inline bool operator==(const InputElementDesc& a, const InputElementDesc& b)
{
    return strcmp(a.semanticName, b.semanticName) == 0 &&
        a.semanticIndex == b.semanticIndex &&
        a.format == b.format &&
        a.inputSlot == b.inputSlot &&
        a.alignedByteOffset == b.alignedByteOffset &&
        a.inputSlotClass == b.inputSlotClass &&
        a.instanceDataStepRate == b.instanceDataStepRate;
}

struct Viewport
{
    float topLeftX;
    float topLeftY;
    float width;
    float height;
    float minDepth;
    float maxDepth;

    Viewport() = default;
    Viewport(uint32_t w, uint32_t h)
        : topLeftX(0.0f)
        , topLeftY(0.0f)
        , width((float)w)
        , height((float)h)
        , minDepth(0.0f)
        , maxDepth(1.0f)
    {}
};

struct ScissorRect
{
    uint32_t left;
    uint32_t top;
    uint32_t right;
    uint32_t bottom;
};

enum class RenderResourceFlags : uint8_t
{
    NONE = 0u,
    SRV = (1u << 0u),
    UAV = (1u << 1u),
    RTV = (1u << 2u),
    DSV = (1u << 3u),
};
IMPLEMENT_FLAGS(RenderResourceFlags, uint8_t)

enum class ResourceUsage : uint8_t
{
    DEFAULT,
    STAGING,
};

enum class ResourceTransitionState : uint16_t
{
    COMMON = (0u),
    VERTEX_CONSTANT_BUFFER = (1u << 0u),
    INDEX_BUFFER = (1u << 1u),
    RENDER_TARGET = (1u << 2u),
    UNORDERED_ACCESS = (1u << 3u),
    DEPTH_WRITE = (1u << 4u),
    DEPTH_READ = (1u << 5u),
    NON_PIXEL_SHADER_RESOURCE = (1u << 6u),
    PIXEL_SHADER_RESOURCE = (1u << 7u),
    INDIRECT_ARGUMENT = (1u << 9u),
    COPY_DEST = (1u << 10u),
    COPY_SRC = (1u << 11u),
    READ = (VERTEX_CONSTANT_BUFFER | INDEX_BUFFER | NON_PIXEL_SHADER_RESOURCE | PIXEL_SHADER_RESOURCE| INDIRECT_ARGUMENT | COPY_SRC),
    ALL_SHADER_RESOURCE  = (NON_PIXEL_SHADER_RESOURCE | PIXEL_SHADER_RESOURCE),
    PRESENT = 0,    
};
IMPLEMENT_FLAGS(ResourceTransitionState, uint16_t)

}