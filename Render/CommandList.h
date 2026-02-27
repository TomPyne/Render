#pragma once

#include "RootSignature.h"
#include "RenderTypes.h"
#include "PipelineState.h"

#include <memory>

namespace rl
{

enum class CommandListType : uint8_t
{
	GRAPHICS,
	COMPUTE,
	COPY
};

struct CommandList;
struct CommandListImpl;

typedef std::shared_ptr<CommandList> CommandListPtr;

struct CommandListSubmissionGroup
{
	explicit CommandListSubmissionGroup(CommandListType type)
		: Type(type)
	{}

	CommandList* CreateCommandList();

	void Submit();

private:
	CommandListType Type;
	std::vector<std::unique_ptr<CommandList>> CommandLists;
};

struct CommandList
{
	CommandList() = delete;
	CommandList(CommandListImpl* cl); // Takes ownership of pointer.
	CommandList(CommandListImpl* cl, CommandListType type); // Takes ownership of pointer.
	CommandList(const CommandList&) = delete;
	~CommandList();

	void SetRootSignature();
	void SetRootSignature(RootSignature_t rs);
	void SetComputeRootSignature(RootSignature_t rs);

	void ClearRenderTarget(RenderTargetView_t rtv, const float col[4]);
	void ClearDepth(DepthStencilView_t dsv, float depth);

	// Render pass set up
	void SetRenderTargets(const RenderTargetView_t* const rtvs, size_t num, DepthStencilView_t dsv);
	void SetViewports(const Viewport* const vp, size_t num);
	void SetDefaultScissor();
	void SetScissors(const ScissorRect* const scissors, size_t num);

	void SetPipelineState(GraphicsPipelineState_t pso);
	void SetPipelineState(ComputePipelineState_t pso);
	void SetPipelineState(RaytracingPipelineState_t pso);

	void SetVertexBuffers(uint32_t startSlot, uint32_t count, const VertexBuffer_t* const vbs, const uint32_t* const strides, const uint32_t* const offsets);
	void SetVertexBuffers(uint32_t startSlot, uint32_t count, const DynamicBuffer_t* const vbs, const uint32_t* const strides, const uint32_t* const offsets);
	void SetVertexBuffer(uint32_t slot, VertexBuffer_t vb, uint32_t stride, uint32_t offset);
	void SetVertexBuffer(uint32_t slot, DynamicBuffer_t vb, uint32_t stride, uint32_t offset);
	void SetIndexBuffer(IndexBuffer_t ib, RenderFormat format, uint32_t indexOffset);
	void SetIndexBuffer(DynamicBuffer_t ib, RenderFormat format, uint32_t indexOffset);

	void CopyTexture(Texture_t dst, Texture_t src);

	// Drawing
	void DrawIndexedInstanced(uint32_t numIndices, uint32_t numInstances, uint32_t startIndex, uint32_t startVertex, uint32_t startInstance);
	void DrawInstanced(uint32_t numVerts, uint32_t numInstances, uint32_t startVertex, uint32_t startInstance);

	// Indirect commands
	void ExecuteIndirect(IndirectCommand_t ic, StructuredBuffer_t argBuffer, uint64_t argBufferOffset = 0u);

	// Compute commands
	void Dispatch(uint32_t x, uint32_t y, uint32_t z);

	// Mesh Shaders
	void DispatchMesh(uint32_t x, uint32_t y, uint32_t z);

	// Raytracing
	void BuildRaytracingScene(RaytracingScene_t scene);
	void DispatchRays(RaytracingShaderTable_t ShaderTable, uint32_t X, uint32_t Y, uint32_t Z);

	// Dx11 Style Bind Commands
	void BindVertexSRVs(uint32_t startSlot, uint32_t count, const ShaderResourceView_t* const srvs);
	void BindVertexCBVs(uint32_t startSlot, uint32_t count, const ConstantBuffer_t* const cbvs);
	void BindVertexCBVs(uint32_t startSlot, uint32_t count, const DynamicBuffer_t* const cbvs);

	void BindGeometryCBVs(uint32_t startSlot, uint32_t count, const ConstantBuffer_t* const cbvs);
	void BindGeometryCBVs(uint32_t startSlot, uint32_t count, const DynamicBuffer_t* const cbvs);

	void BindPixelSRVs(uint32_t startSlot, uint32_t count, const ShaderResourceView_t* const srvs);
	void BindPixelCBVs(uint32_t startSlot, uint32_t count, const ConstantBuffer_t* const cbvs);
	void BindPixelCBVs(uint32_t startSlot, uint32_t count, const DynamicBuffer_t* const cbvs);

	void BindComputeSRVs(uint32_t startSlot, uint32_t count, const ShaderResourceView_t* const srvs);
	void BindComputeUAVs(uint32_t startSlot, uint32_t count, const UnorderedAccessView_t* const srvs);
	void BindComputeCBVs(uint32_t startSlot, uint32_t count, const ConstantBuffer_t* const cbvs);
	void BindComputeCBVs(uint32_t startSlot, uint32_t count, const DynamicBuffer_t* const cbvs);

	// Bindless Command
	void SetGraphicsRootValue(uint32_t slot, uint32_t offset, uint32_t value);
	void SetComputeRootValue(uint32_t slot, uint32_t offset, uint32_t value);

	void SetGraphicsRootCBV(uint32_t slot, ConstantBuffer_t cb);
	void SetComputeRootCBV(uint32_t slot, ConstantBuffer_t cb);
	void SetGraphicsRootCBV(uint32_t slot, DynamicBuffer_t cb);
	void SetComputeRootCBV(uint32_t slot, DynamicBuffer_t cb);

	void SetGraphicsRootSRV(uint32_t slot, ShaderResourceView_t srv);
	void SetComputeRootSRV(uint32_t slot, ShaderResourceView_t srv);
	void SetComputeRootSRV(uint32_t slot, RaytracingScene_t srv);

	void SetGraphicsRootUAV(uint32_t slot, UnorderedAccessView_t uav);
	void SetComputeRootUAV(uint32_t slot, UnorderedAccessView_t uav);

	void SetGraphicsRootDescriptorTable(uint32_t slot);
	void SetComputeRootDescriptorTable(uint32_t slot);

	void TransitionResource(Texture_t tex, ResourceTransitionState before, ResourceTransitionState after);
	void TransitionResource(StructuredBuffer_t buf, ResourceTransitionState before, ResourceTransitionState after);
	void UAVBarrier(Texture_t tex);
	void UAVBarrier(StructuredBuffer_t buf);

	// Events and markers
	void BeginEvent(const char* EventStr);
	void BeginEvent(const wchar_t* EventStr);
	void EndEvent();
	void AddMarker(const char* MarkerStr);
	void AddMarker(const wchar_t* MarkerStr);

	static CommandListPtr Create();
	static CommandListPtr Create(CommandListType type);
	static CommandList* CreateRaw(CommandListType type);
	static void Execute(CommandListPtr& cl);
	static void ExecuteAndStall(CommandListPtr& cl);

	static void ReleaseAll();

	CommandListImpl* GetCommandListImpl();

public:

	// Helper functions and implementation macro for converting texture arrays into appropriate views.

#define BIND_HELPER_IMPL(FuncName, Type, GetFunc, SetFunc)			\
	template<uint32_t COUNT>										\
	void FuncName(uint32_t startSlot, Texture_t(&textures)[COUNT])	\
	{																\
		Type views[COUNT];											\
		for (uint32_t i = 0; i < COUNT; i++)						\
			views[i] = GetFunc(textures[i]);						\
		SetFunc(startSlot, COUNT, views);							\
	}																\

	BIND_HELPER_IMPL(BindTexturesAsVertexSRVs, ShaderResourceView_t, GetTextureSRV, BindVertexSRVs);
	BIND_HELPER_IMPL(BindTexturesAsPixelSRVs, ShaderResourceView_t, GetTextureSRV, BindPixelSRVs);
	BIND_HELPER_IMPL(BindTexturesAsComputeSRVs, ShaderResourceView_t, GetTextureSRV, BindComputeSRVs);
	BIND_HELPER_IMPL(BindTexturesAsComputeUAVs, UnorderedAccessView_t, GetTextureUAV, BindComputeUAVs);

private:
	std::unique_ptr<CommandListImpl> Impl;

	CommandListType Type = CommandListType::GRAPHICS;

	void Begin();
	void Finish();
};

struct CommandListEventScope
{
	explicit CommandListEventScope(CommandList* InCL, const char* Str)
	{		
		if (Str && InCL)
		{
			CL = InCL;
			CL->BeginEvent(Str);
		}
	}

	explicit CommandListEventScope(CommandList* InCL, const wchar_t* Str)
	{
		if (Str && InCL)
		{
			CL = InCL;
			CL->BeginEvent(Str);
		}
	}

	~CommandListEventScope()
	{
		if (CL)
		{
			CL->EndEvent();
		}
	}

private:
	CommandList* CL = nullptr;
};

}