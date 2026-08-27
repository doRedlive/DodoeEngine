// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

	enum class RenderPhase : UInt8 {
	    Opaque,
	    Shadow,
	    Skybox,
	    Lighting,
	    Decals,
	    Transparent,
	    Sprite,
	    PostProcess,
	    UI,
	    EditorGizmo,
	    DebugUI,
	    Present,
	};

} // namespace dodoe
