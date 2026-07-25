// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

	enum class RenderPhase : UInt8 {
	    Shadow,
	    GBuffer,
	    DeferredLighting,
	    Skybox,
	    Decals,
	    Forward,
	    Sprite,
	    PostProcess,
	    EditorGizmo,
	    DebugUI,
	    Present,
	};

} // namespace dodoe
