#include "render_system_bridge.h"

#include "runtime/core/application.h"
#include "runtime/function/render/render_system.h"

namespace dodoe {

    namespace {

        RenderSystem* TryGetRenderSystem() {
            return Application::Self().context().render_system.get();
        }

    } // namespace

    RenderScene& RenderSceneSyncScope::scene() const {
        DO_ASSERT(m_render_scene != nullptr, "RenderSceneSyncScope requires a valid render scene");
        return *m_render_scene;
    }

    void RenderSceneSyncScope::flushIfDirty(const Bool dirty) const {
        if (dirty && m_render_scene) {
            m_render_scene->flushUpdates();
        }
    }

    RenderSceneSyncScope TryBeginRenderSceneSync() {
        auto* render_system = TryGetRenderSystem();
        if (!render_system) {
            return {};
        }
        return RenderSceneSyncScope(&render_system->getRenderScene());
    }

    Camera* TryGetMainCamera() {
        auto* render_system = TryGetRenderSystem();
        return render_system ? &render_system->getMainCamera() : nullptr;
    }

    TextureManager* TryGetTextureManager() {
        auto* render_system = TryGetRenderSystem();
        return render_system ? render_system->getTextureManager() : nullptr;
    }

    void SubmitMainCameraViewProjection(const Matrix4f& view_proj_matrix, const Vector3f& position) {
        auto* render_system = TryGetRenderSystem();
        if (!render_system) {
            return;
        }
        render_system->submitMainCameraViewProjection(view_proj_matrix, position);
    }

} // dodoe
