// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

	enum class RenderPhase : UInt8 {
	    Shadow,
	    Opaque,
	    Skybox,
	    Lighting,
	    Decals,
	    Transparent,
	    Sprite,
	    Resolve,
	    Taa,
	    PostProcess,
	    UI,
	    EditorGizmo,
	    DebugUI,
	    Present,
	};

} // namespace dodoe
