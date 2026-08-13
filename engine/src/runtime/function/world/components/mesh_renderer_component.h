// Created by Redlive on 2026/4/7.
#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/object/pptr.h"
#include "runtime/function/render/render_scene/primitive_scene_info.h"
#include "runtime/function/render/material/material.h"

namespace dodoe {
    class Mesh;
}

REFLECTION_TYPE(MeshRendererComponent)

namespace dodoe {

	STRUCT(MeshRendererComponent, WhiteListFields, ScriptBind) {
		REFLECTION_BODY(MeshRendererComponent)

        META(Enable)
        PPtr<Mesh> mesh;
        META(Enable)
        Int32 section_index{0};
        META(Enable)
        DynamicArray<PPtr<Material>> override_materials{};
        META(Enable)
        bool visible{true};
        META(Enable)
        bool cast_shadow{true};
        META(Enable)
        PrimitiveMobility mobility{PrimitiveMobility::Static};
        bool dirty{true};

	};

} // dodoe
