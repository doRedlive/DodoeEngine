#pragma once

#include "dopch.h"

#include "render_scene/render_scene.h"

namespace dodoe {

    class Camera;
    class TextureManager;

    struct RendererCreateInfo {
        gfx::DeviceHandle device{nullptr};
        Camera* main_camera{nullptr};
        TextureManager* texture_manager{nullptr};
    };

    class Renderer : public Managed<Renderer, RendererCreateInfo> {
        friend class Managed<Renderer, RendererCreateInfo>;

        Scope<RenderScene> m_render_scene{nullptr};
        Camera* m_main_camera{nullptr};
        TextureManager* m_texture_manager{nullptr};

    public:
        Renderer() = default;
        ~Renderer() = default;

        void setMainCameraViewProjection(const Matrix4f& view_proj_matrix, const Vector3f& position);
        void addRenderObject(Uuid entity_uuid, const Matrix4f& world_transform, Scope<RenderObject> render_object);
        void updateRenderObjectTransform(Uuid entity_uuid, const Matrix4f& world_transform);
        void removeRenderObject(Uuid entity_uuid);
        void addLightObject(Uuid entity_uuid, const Matrix4f& world_transform, const RenderLightObject& light);
        void updateLightTransform(Uuid entity_uuid, const Matrix4f& world_transform);
        void removeLightObject(Uuid entity_uuid);
        void flushSceneUpdates();
        void prepareBuffers(const gfx::CommandListHandle& cmd_list);

        [[nodiscard]] Bool hasRenderObject(Uuid entity_uuid) const;
        [[nodiscard]] Bool hasLightObject(Uuid entity_uuid) const;
        [[nodiscard]] const RenderObject* findRenderObject(Uuid entity_uuid) const;
        [[nodiscard]] const RenderLightObject* findLightObject(Uuid entity_uuid) const;
        [[nodiscard]] Camera* getMainCamera() const { return m_main_camera; }
        [[nodiscard]] TextureManager* getTextureManager() const { return m_texture_manager; }
        [[nodiscard]] const RenderScene& getRenderScene() const { return *m_render_scene.get(); }
        [[nodiscard]] RenderScene& getRenderScene() { return *m_render_scene.get(); }
        [[nodiscard]] gfx::TextureHandle getSkyboxTexture() const;
        void setSkyboxTexture(const gfx::TextureHandle& skybox_texture);

    private:
        [[nodiscard]] bool initialize(const RendererCreateInfo& info);
        void shutdown();
    };

} // dodoe
