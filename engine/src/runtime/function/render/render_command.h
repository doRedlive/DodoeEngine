// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/render/texture/texture.h"
#include "render_scene/primitive_render_object.h"
#include "render_scene/sprite_render_object.h"
#include "render_scene/light_scene_info.h"
#include "render_scene/ui_scene_info.h"

namespace dodoe {

    enum class SceneCommandType : UInt8 {
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

    struct SceneCommand {
        SceneCommandType type{SceneCommandType::None};
        UUID id{};
        Matrix4f transform{1.0f};
        Scope<PrimitiveRenderObject> primitive{};
        LightSceneInfo light{};
        Scope<SpriteRenderObject> sprite{};
        DynamicArray<UISceneInfo> ui_scene_infos{};
    };

    enum class ResourceCommandType : UInt8 {
        None = 0,
        CreateTexture,
        CreateBuffer,
    };

    struct ResourceCommand {
        ResourceCommandType type{ResourceCommandType::None};
        Scope<Texture2D> texture_object{};
        Bool texture_is_hdr{false};
        GfxBufferHandle buffer{};
        GfxBufferDesc buffer_desc{};
        DynamicArray<UInt8> resource_data{};
    };

} // dodoe
