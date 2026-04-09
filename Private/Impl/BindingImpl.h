#pragma once

#include "Binding.h"

namespace rl
{
enum class TextureDimension : uint8_t;

bool CreateTextureSRVImpl(ShaderResourceView_t srv, Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t mipLevels, uint32_t depthOrArraySize);
bool CreateTextureUAVImpl(UnorderedAccessView_t uav, Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize, uint32_t mipSlice);
bool CreateTextureRTVImpl(RenderTargetView_t rtv, Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize);
bool CreateTextureDSVImpl(DepthStencilView_t dsv, Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize);

void BindTextureSRVImpl(ShaderResourceView_t srv, Texture_t tex);
void BindTextureUAVImpl(UnorderedAccessView_t uav, Texture_t tex);
void BindTextureRTVImpl(RenderTargetView_t rtv, Texture_t tex);
void BindTextureDSVImpl(DepthStencilView_t dsv, Texture_t tex);

bool CreateStructuredBufferSRVImpl(ShaderResourceView_t srv, StructuredBuffer_t buf, uint32_t firstElement, uint32_t numElements, uint32_t structureByteStride);
bool CreateStructuredBufferUAVImpl(UnorderedAccessView_t uav, StructuredBuffer_t buf, uint32_t firstElement, uint32_t numElements, uint32_t structureByteStride);

void DestroySRV(ShaderResourceView_t srv);
void DestroyUAV(UnorderedAccessView_t uav);
void DestroyRTV(RenderTargetView_t rtv);
void DestroyDSV(DepthStencilView_t dsv);

uint32_t GetDescriptorIndexImpl(ShaderResourceView_t srv);
uint32_t GetDescriptorIndexImpl(UnorderedAccessView_t uav);

}