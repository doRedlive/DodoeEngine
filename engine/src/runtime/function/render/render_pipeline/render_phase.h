// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

	enum class RenderPhase : UInt8 {
	    GBuffer,
	    Shadow,
	    Skybox,
	    DeferredLighting,
	    Decals,
	    Forward,
	    Sprite,
	    PostProcess,
	    UI,
	    EditorGizmo,
	    DebugUI,
	    Present,
	};

} // namespace dodoe
