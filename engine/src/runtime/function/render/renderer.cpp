#include "renderer.h"

namespace dodoe {

    Bool Renderer::Initialize(Camera* const camera, TextureManager* const texture_manager) {
        s_main_camera = camera;
        s_texture_manager = texture_manager;
        s_render_scene = RenderScene::Create({});
        return s_render_scene != nullptr;
    }

    void Renderer::Shutdown() {
        RenderScene::Destroy(s_render_scene);
        s_render_scene = nullptr;
        s_main_camera = nullptr;
        s_texture_manager = nullptr;
    }

    void Renderer::SetMainCameraViewProjection(const Matrix4f& view_proj_matrix, const Vector3f& position) {
        if (s_render_scene) {
            s_render_scene->setMainCameraViewProjection(view_proj_matrix, position);
        }
    }

    void Renderer::AddPrimitive(Scope<PrimitiveRenderObject> primitive) {
        if (s_render_scene) {
            s_render_scene->addPrimitive(std::move(primitive));
        }
    }

    void Renderer::UpdatePrimitiveTransform(const UUID id, const Matrix4f& world_transform) {
        if (s_render_scene) {
            s_render_scene->updatePrimitiveTransform(id, world_transform);
        }
    }

    void Renderer::RemovePrimitive(const UUID id) {
        if (s_render_scene) {
            s_render_scene->removePrimitive(id);
        }
    }

    void Renderer::AddLight(Scope<LightRenderObject> light) {
        if (s_render_scene) {
            s_render_scene->addLight(std::move(light));
        }
    }

    void Renderer::UpdateLightTransform(const UUID id, const Matrix4f& world_transform) {
        if (s_render_scene) {
            s_render_scene->updateLightTransform(id, world_transform);
        }
    }

    void Renderer::RemoveLight(const UUID id) {
        if (s_render_scene) {
            s_render_scene->removeLight(id);
        }
    }

    void Renderer::AddSprite(Scope<SpriteRenderObject> sprite) {
        if (s_render_scene) {
            s_render_scene->addSprite(std::move(sprite));
        }
    }

    void Renderer::UpdateSpriteTransform(const UUID id, const Matrix4f& world_transform) {
        if (s_render_scene) {
            s_render_scene->updateSpriteTransform(id, world_transform);
        }
    }

    void Renderer::RemoveSprite(const UUID id) {
        if (s_render_scene) {
            s_render_scene->removeSprite(id);
        }
    }

    void Renderer::FlushSceneUpdates() {
        if (s_render_scene) {
            s_render_scene->flushUpdates();
        }
    }

    Bool Renderer::HasPrimitive(const UUID id) {
        return s_render_scene ? s_render_scene->hasPrimitive(id) : false;
    }

    Bool Renderer::HasLight(const UUID id) {
        return s_render_scene ? s_render_scene->hasLight(id) : false;
    }

    const PrimitiveRenderObject* Renderer::FindPrimitive(const UUID id) {
        return s_render_scene ? s_render_scene->findPrimitive(id) : nullptr;
    }

    const LightRenderObject* Renderer::FindLight(const UUID id) {
        return s_render_scene ? s_render_scene->findLight(id) : nullptr;
    }

    RenderScene& Renderer::GetRenderScene() {
        DO_ASSERT(s_render_scene != nullptr, "Renderer::GetRenderScene: not initialized");
        return *s_render_scene;
    }

    Camera* Renderer::GetMainCamera() {
        return s_main_camera;
    }

    TextureManager* Renderer::GetTextureManager() {
        return s_texture_manager;
    }

} // dodoe
