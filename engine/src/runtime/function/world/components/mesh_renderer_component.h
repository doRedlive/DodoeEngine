// Created by Redlive on 2026/4/7.
#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/object/pptr.h"
#include "runtime/function/render/render_scene/primitive_scene_info.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"
#include "runtime/function/render/mesh_draw/mesh_data.h"
#include "runtime/function/render/material/material.h"
#include "runtime/function/animation/skeleton.h"
#include "runtime/core/math/math.h"

REFLECTION_TYPE(MeshRendererComponent)

namespace dodoe {

	STRUCT(MeshRendererComponent, WhiteListFields, ScriptBind) {
		REFLECTION_BODY(MeshRendererComponent)

        META(Enable)
		MeshUploadData upload_data;
        META(Enable)
        DynamicArray<MeshLODData> lods;
        META(Enable)
        DynamicArray<MaterialProperties> override_materials{};
        META(Enable)
        bool visible{true};
        META(Enable)
        bool cast_shadow{true};
        META(Enable)
        PrimitiveMobility mobility{PrimitiveMobility::Static};
        Ref<Skeleton> skeleton{};
        DynamicArray<Matrix4f> skinning_matrices{};
        bool dirty{true};

	};

} // dodoe
