#pragma once

#include "RenderTypes.h"

// TODO:
// Add support for byte buffers
//	I think the best approach for byte buffers would be to create a GenericBuffer_t type with BufferUsage flags passed to it telling it what kind of buffer
//	it can be. Currently we can't have a struct buf and a vertex buf mapped to the same memory, even though this is possible in our APIs.
//	As well as having explicitly typed buffers having one that we can reinterpret in different ways would be a lot more flexible.
//	The explicit buffer creation code could then just create a generic buffer with fixed flags, then perhaps the returned explicit buffers are just casted from the generic buffer ID.

namespace rl
{

RENDER_TYPE(DynamicBuffer_t);

VertexBuffer_t CreateVertexBuffer(const void* const data, size_t size);
IndexBuffer_t CreateIndexBuffer(const void* const data, size_t size);
StructuredBuffer_t CreateStructuredBuffer(const void* const data, size_t size, size_t stride, RenderResourceFlags flags);
ConstantBuffer_t CreateConstantBuffer(const void* const data, size_t size);

DynamicBuffer_t CreateDynamicVertexBuffer(const void* const data, size_t size);
DynamicBuffer_t CreateDynamicIndexBuffer(const void* const data, size_t size);
DynamicBuffer_t CreateDynamicConstantBuffer(const void* const data, size_t size);
DynamicBuffer_t CreateDynamicByteBuffer(const void* const data, size_t size);

void UpdateVertexBuffer(VertexBuffer_t vb, const void* const data, size_t size);
void UpdateIndexBuffer(IndexBuffer_t ib, const void* const data, size_t size);
void UpdateConstantBuffer(ConstantBuffer_t cb, const void* const data, size_t size);
void UpdateStructuredBuffer(StructuredBuffer_t sb, const void* const data, size_t size);

template<typename T> inline VertexBuffer_t CreateVertexBufferFromArray(const T* const data, size_t count) { return CreateVertexBuffer(data, sizeof(T) * count); }
template<typename T> inline IndexBuffer_t CreateIndexBufferFromArray(const T* const data, size_t count) { return CreateIndexBuffer(data, sizeof(T) * count); }
template<typename T> inline StructuredBuffer_t CreateStructuredBuffer(const T* const data, size_t count) { return CreateStructuredBuffer(data, sizeof(T) * count, sizeof(T), RenderResourceFlags::SRV); }
template<typename T> inline StructuredBuffer_t CreateRWStructuredBuffer(const T* const data, size_t count) { return CreateStructuredBuffer(data, sizeof(T) * count, sizeof(T), RenderResourceFlags::UAV); }
template<typename T> inline ConstantBuffer_t CreateConstantBuffer(const T* const data) { return CreateConstantBuffer(data, sizeof(T)); }

template<typename T> inline DynamicBuffer_t CreateDynamicVertexBufferFromArray(const T* const data, size_t count) { return CreateDynamicVertexBuffer(data, sizeof(T) * count); }
template<typename T> inline DynamicBuffer_t CreateDynamicIndexBufferFromArray(const T* const data, size_t count) { return CreateDynamicIndexBuffer(data, sizeof(T) * count); }
template<typename T> inline DynamicBuffer_t CreateDynamicConstantBuffer(const T* const data) { return CreateDynamicConstantBuffer(data, sizeof(T)); }
template<typename T> inline DynamicBuffer_t CreateDynamicByteBufferFromArray(const T* const data, size_t count) { return CreateDynamicByteBuffer(data, sizeof(T) * count); }

template<typename T> inline void UpdateVertexBufferFromArray(VertexBuffer_t vb, const T* const data, size_t count) { UpdateVertexBuffer(vb, data, sizeof(T) * count); }
template<typename T> inline void UpdateIndexBufferFromArray(IndexBuffer_t ib, const T* const data, size_t count) { UpdateIndexBuffer(ib, data, sizeof(T) * count); }
template<typename T> inline void UpdateConstantBuffer(ConstantBuffer_t cb, const T* const data) { UpdateConstantBuffer(cb, sizeof(T)); }
template<typename T> inline void UpdateStructuredBufferFromArray(StructuredBuffer_t sb, const T* const data, size_t count) { UpdateStructuredBuffer(sb, data, sizeof(T) * count); }

void DynamicBuffers_NewFrame();
void DynamicBuffers_EndFrame();

struct ConstantUploadSpan_s
{
	const void* Data = nullptr;
	size_t Size = 0;
	size_t DstOffset = 0; // Must be 256 byte aligned
};

// Copies the spans into a GPU local constant buffer that is valid until the end of the frame
GPUAddress_t UploadFrameConstants(CommandList* cl, const ConstantUploadSpan_s* spans, size_t numSpans, size_t totalSize);

struct CommandList;
void UploadBuffers(CommandList* cl);

size_t GetVertexBufferCount();
size_t GetIndexBufferCount();
size_t GetStructuredBufferCount();
size_t GetConstantBufferCount();
size_t GetFrameConstantPageCount();

}