#pragma once

#include "dopch.h"

#include "render_scene/render_scene.h"

namespace dodoe {

    class Camera;
    class TextureManager;

    class Renderer {
        inline static Scope<RenderScene> s_render_scene{nullptr};
        inline static Camera* s_main_camera{nullptr};
        inline static TextureManager* s_texture_manager{nullptr};

    public:
        Renderer() = delete;

        static Bool Initialize(Camera* camera, TextureManager* texture_manager);
        static void Shutdown();

        static void SetMainCameraViewProjection(const Matrix4f& view_proj_matrix, const Vector3f& position);
        static void AddPrimitive(Scope<PrimitiveRenderObject> primitive);
        static void UpdatePrimitiveTransform(UUID id, const Matrix4f& world_transform);
        static void RemovePrimitive(UUID id);
        static void AddLight(Scope<LightRenderObject> light);
        static void UpdateLightTransform(UUID id, const Matrix4f& world_transform);
        static void RemoveLight(UUID id);
        static void AddSprite(Scope<SpriteRenderObject> sprite);
        static void UpdateSpriteTransform(UUID id, const Matrix4f& world_transform);
        static void RemoveSprite(UUID id);
        static void FlushSceneUpdates();

        [[nodiscard]] static Bool HasPrimitive(UUID id);
        [[nodiscard]] static Bool HasLight(UUID id);
        [[nodiscard]] static const PrimitiveRenderObject* FindPrimitive(UUID id);
        [[nodiscard]] static const LightRenderObject* FindLight(UUID id);

        [[nodiscard]] static RenderScene& GetRenderScene();
        [[nodiscard]] static Camera* GetMainCamera();
        [[nodiscard]] static TextureManager* GetTextureManager();
    };

} // dodoe
