#pragma once

#include "RenderTypes.h"

namespace rl
{
RENDER_TYPE(ShaderResourceView_t);
RENDER_TYPE(UnorderedAccessView_t);
RENDER_TYPE(RenderTargetView_t);
RENDER_TYPE(DepthStencilView_t);

FWD_RENDER_TYPE(Texture_t);
FWD_RENDER_TYPE(StructuredBuffer_t);

enum class TextureDimension : uint8_t;

struct TextureCreateDescEx;

ShaderResourceView_t CreateTextureSRV(Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t mipLevels, uint32_t depthOrArraySize);
UnorderedAccessView_t CreateTextureUAV(Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize, uint32_t mipSlice = 0u);
RenderTargetView_t CreateTextureRTV(Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize);
DepthStencilView_t CreateTextureDSV(Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize);

// Determine view parameters using the texture descriptors. Since DSVs always use a special format you need to provide a format.
// Otherwise, if you want to specify a custom format, use the verbose create function.
ShaderResourceView_t CreateTextureSRV(Texture_t tex);
UnorderedAccessView_t CreateTextureUAV(Texture_t tex);
RenderTargetView_t CreateTextureRTV(Texture_t tex);
DepthStencilView_t CreateTextureDSV(Texture_t tex, RenderFormat depthFormat);

ShaderResourceView_t CreateStructuredBufferSRV(StructuredBuffer_t buf, uint32_t firstElem, uint32_t numElems, uint32_t stride);
UnorderedAccessView_t CreateStructuredBufferUAV(StructuredBuffer_t buf, uint32_t firstElem, uint32_t numElems, uint32_t stride);

//TODO: Buffer SRV and UAV
ShaderResourceView_t AllocSRV(RenderFormat format, TextureDimension dim, uint32_t mipLevels, uint32_t depthOrArraySize);
UnorderedAccessView_t AllocUAV(RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize);
RenderTargetView_t AllocRTV(RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize);
DepthStencilView_t AllocDSV(RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize);

void BindSRV(ShaderResourceView_t srv, Texture_t texture);
void BindUAV(UnorderedAccessView_t uav, Texture_t texture);
void BindRTV(RenderTargetView_t rtv, Texture_t texture);
void BindDSV(DepthStencilView_t dsv, Texture_t texture);

RenderFormat GetSRVFormat(ShaderResourceView_t srv);
RenderFormat GetUAVFormat(UnorderedAccessView_t uav);
RenderFormat GetRTVFormat(RenderTargetView_t rtv);
RenderFormat GetDSVFormat(DepthStencilView_t dsv);

void RenderRef(ShaderResourceView_t srv);
void RenderRef(UnorderedAccessView_t uav);
void RenderRef(RenderTargetView_t rtv);
void RenderRef(DepthStencilView_t dsv);

void RenderRelease(ShaderResourceView_t srv);
void RenderRelease(UnorderedAccessView_t uav);
void RenderRelease(RenderTargetView_t rtv);
void RenderRelease(DepthStencilView_t dsv);

size_t GetShaderResourceViewCount();
size_t GetUnorderedAccessViewCount();
size_t GetRenderTargetViewCount();
size_t GetDepthStencilViewCount();

uint32_t GetDescriptorIndex(ShaderResourceView_t srv);
uint32_t GetDescriptorIndex(UnorderedAccessView_t uav);
}