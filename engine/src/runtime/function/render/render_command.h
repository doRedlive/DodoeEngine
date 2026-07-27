// do@Redlive

#pragma once

#include "dopch.h"

#include "render_scene/primitive_render_object.h"
#include "render_scene/sprite_render_object.h"
#include "render_scene/light_scene_info.h"
#include "render_scene/ui_scene_info.h"

namespace dodoe {

    enum class RenderCommandType : UInt8 {
        None = 0,
        AddPrimitive,
        RemovePrimitive,
        UpdatePrimitiveTransform,
        AddLight,
        RemoveLight,
        UpdateLightTransform,
        AddSprite,
        RemoveSprite,
        UpdateSpriteTransform,
        SubmitUIBatch,
    };

    struct RenderCommand {
        RenderCommandType type{RenderCommandType::None};
        UUID id{};
        Matrix4f transform{1.0f};
        Scope<PrimitiveRenderObject> primitive{};
        LightSceneInfo light{};
        Scope<SpriteRenderObject> sprite{};
        DynamicArray<UISceneInfo> ui_scene_infos{};
    };

} // dodoe
