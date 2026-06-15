// Created by Redlive on 2026/4/7.
#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/object/pptr.h"
#include "runtime/function/render/framework/primitive_scene_info.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"
#include "runtime/function/render/mesh_draw/mesh_data.h"

REFLECTION_TYPE(MeshRendererComponent)

namespace dodoe {

	STRUCT(MeshRendererComponent, WhiteListFields) {
		REFLECTION_BODY(MeshRendererComponent)

        META(Enable)
		MeshUploadData upload_data;
        META(Enable)
        DynamicArray<MeshLODData> lods;
        META(Enable)
        DynamicArray<Ref<Material>> override_materials{};
        META(Enable)
        bool visible{true};
        META(Enable)
        bool cast_shadow{true};
        META(Enable)
        PrimitiveMobility mobility{PrimitiveMobility::Static};
        bool dirty{true};

	};

} // dodoe
