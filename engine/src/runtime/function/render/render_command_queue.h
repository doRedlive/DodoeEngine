// do@Redlive

#pragma once

#include "dopch.h"

#include "render_scene/render_scene.h"
#include "render_scene/light_scene_info.h"
#include "render_view/render_view.h"

namespace dodoe {

    class DODOE_API RenderCommandQueue {
    public:
        RenderCommandQueue() = delete;

        static void AddPrimitive(Scope<PrimitiveRenderObject> primitive);
        static void RemovePrimitive(UUID id);
        static void UpdatePrimitiveTransform(UUID id, const Matrix4f& world_transform);

        static void AddLight(LightSceneInfo&& info);
        static void RemoveLight(UUID id);
        static void UpdateLightTransform(UUID id, const Matrix4f& world_transform);

        static void AddSprite(Scope<SpriteRenderObject> sprite);
        static void RemoveSprite(UUID id);
        static void UpdateSpriteTransform(UUID id, const Matrix4f& world_transform);

        static void SubmitUI(DynamicArray<UISceneInfo> instances);
    };

} // dodoe
