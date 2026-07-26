#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/function/render/render_scene/primitive_scene_info.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"
#include "runtime/function/render/mesh_draw/mesh_data.h"
#include "runtime/function/render/material/material.h"

REFLECTION_TYPE(FoliageRendererInstance)
REFLECTION_TYPE(FoliageRendererComponent)

namespace dodoe {

    STRUCT(FoliageRendererInstance, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(FoliageRendererInstance)

        META(Enable)
        Vector3f position{0.0f, 0.0f, 0.0f};
        META(Enable)
        Vector3f rotation{0.0f, 0.0f, 0.0f};
        META(Enable)
        Vector3f scale{1.0f, 1.0f, 1.0f};
        META(Enable)
        Color color_tint{Color::white()};
        META(Enable)
        float wind_phase{0.0f};
        META(Enable)
        float variation{0.0f};
    };

    STRUCT(FoliageRendererComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(FoliageRendererComponent)

        META(Enable)
        MeshUploadData upload_data;
        META(Enable)
        DynamicArray<MeshLODData> lods;
        META(Enable)
        DynamicArray<Ref<MaterialProperties>> override_materials{};
        META(Enable)
        PrimitiveMobility mobility{PrimitiveMobility::Static};
        META(Enable)
        bool visible{true};
        META(Enable)
        bool cast_shadow{true};
        META(Enable)
        Vector3f instance_bounds_extent{0.5f, 0.5f, 0.5f};
        META(Enable)
        DynamicArray<FoliageRendererInstance> instances{};

        bool dirty{true};
    };

} // dodoe
