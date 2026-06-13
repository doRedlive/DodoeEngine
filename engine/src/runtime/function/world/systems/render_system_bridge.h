#pragma once

#include "dopch.h"

namespace dodoe {

    class Camera;
    class RenderScene;
    class TextureManager;

    class RenderSceneSyncScope {
        RenderScene* m_render_scene{nullptr};

    public:
        RenderSceneSyncScope() = default;
        explicit RenderSceneSyncScope(RenderScene* render_scene) : m_render_scene(render_scene) { }

        [[nodiscard]] explicit operator Bool() const { return m_render_scene != nullptr; }
        [[nodiscard]] RenderScene& scene() const;
        void flushIfDirty(Bool dirty) const;
    };

    [[nodiscard]] RenderSceneSyncScope TryBeginRenderSceneSync();
    [[nodiscard]] Camera* TryGetMainCamera();
    [[nodiscard]] TextureManager* TryGetTextureManager();
    void SubmitMainCameraViewProjection(const Matrix4f& view_proj_matrix, const Vector3f& position);

} // dodoe
