// Created by Redlive on 2026/4/7.
#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/resource/asset/asset.h"
#include "runtime/function/render/render_types.h"

REFLECTION_TYPE(MeshRendererComponent)

namespace dodoe {

	STRUCT(MeshRendererComponent, WhiteListFields) {
		REFLECTION_BODY(MeshRendererComponent)

		Ref<Mesh> mesh;
        bool dirty{true};

	};

} // dodoe
