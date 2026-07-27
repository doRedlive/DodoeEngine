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
	    UI,              // do@Redlive — UI 渲染（所有场景内容之后、ImGui 之前）
	    PostProcess,
	    EditorGizmo,
	    DebugUI,
	    Present,
	};

} // namespace dodoe
