#pragma once

#include "RenderDefines.h"

#include "Binding.h"
#include "Buffers.h"
#include "CommandList.h"
#include "IndirectCommands.h"
#include "PipelineState.h"
#include "RenderTypes.h"
#include "RootSignature.h"
#include "Samplers.h"
#include "Shaders.h"
#include "Textures.h"
#include "View.h"

#include "RenderPtr.h"

namespace rl
{

struct RenderInitParams
{
	bool DebugEnabled = false;
	bool GPUValidationEnabled = false;

	std::vector<RenderDebugWarnings> DisabledWarnings;

	RootSignatureDesc RootSigDesc;
};

bool Render_Init(const RenderInitParams& params);
bool Render_Initialised();

// Reset systems access by the frame update
void Render_BeginFrame();

// Begin rendering
void Render_BeginRenderFrame();

void Render_EndFrame();

void Render_ShutDown();

void Render_PushDebugWarningDisable(RenderDebugWarnings warning);
void Render_PopDebugWarningDisable();

bool Render_IsBindless();
bool Render_IsThreadSafe();
bool Render_SupportsMeshShaders();
bool Render_SupportsRaytracing();

const char* Render_ApiId();

}