#include "renderer.h"

namespace dodoe {

    bool Renderer::initialize(const RendererCreateInfo& info) {
        m_main_camera = info.main_camera;
        m_texture_manager = info.texture_manager;
        m_render_scene = RenderScene::Create({info.device});
        return m_render_scene != nullptr;
    }

    void Renderer::shutdown() {
        RenderScene::Destroy(m_render_scene);
        m_main_camera = nullptr;
        m_texture_manager = nullptr;
    }

    void Renderer::setMainCameraViewProjection(const Matrix4f& view_proj_matrix, const Vector3f& position) {
        if (m_render_scene) {
            m_render_scene->setMainCameraViewProjection(view_proj_matrix, position);
        }
    }

    void Renderer::addRenderObject(const Uuid entity_uuid, const Matrix4f& world_transform, Scope<RenderObject> render_object) {
        if (m_render_scene) {
            m_render_scene->addRenderObject(entity_uuid, world_transform, std::move(render_object));
        }
    }

    void Renderer::updateRenderObjectTransform(const Uuid entity_uuid, const Matrix4f& world_transform) {
        if (m_render_scene) {
            m_render_scene->updateRenderObjectTransform(entity_uuid, world_transform);
        }
    }

    void Renderer::removeRenderObject(const Uuid entity_uuid) {
        if (m_render_scene) {
            m_render_scene->removeRenderObject(entity_uuid);
        }
    }

    void Renderer::addLightObject(const Uuid entity_uuid, const Matrix4f& world_transform, const RenderLightObject& light) {
        if (m_render_scene) {
            m_render_scene->addLightObject(entity_uuid, world_transform, light);
        }
    }

    void Renderer::updateLightTransform(const Uuid entity_uuid, const Matrix4f& world_transform) {
        if (m_render_scene) {
            m_render_scene->updateLightTransform(entity_uuid, world_transform);
        }
    }

    void Renderer::removeLightObject(const Uuid entity_uuid) {
        if (m_render_scene) {
            m_render_scene->removeLightObject(entity_uuid);
        }
    }

    void Renderer::flushSceneUpdates() {
        if (m_render_scene) {
            m_render_scene->flushUpdates();
        }
    }

    void Renderer::prepareBuffers(const gfx::CommandListHandle& cmd_list) {
        if (m_render_scene) {
            m_render_scene->prepareBuffers(cmd_list);
        }
    }

    Bool Renderer::hasRenderObject(const Uuid entity_uuid) const {
        return m_render_scene ? m_render_scene->hasRenderObject(entity_uuid) : false;
    }

    Bool Renderer::hasLightObject(const Uuid entity_uuid) const {
        return m_render_scene ? m_render_scene->hasLightObject(entity_uuid) : false;
    }

    const RenderObject* Renderer::findRenderObject(const Uuid entity_uuid) const {
        return m_render_scene ? m_render_scene->findRenderObject(entity_uuid) : nullptr;
    }

    const RenderLightObject* Renderer::findLightObject(const Uuid entity_uuid) const {
        return m_render_scene ? m_render_scene->findLightObject(entity_uuid) : nullptr;
    }

    gfx::TextureHandle Renderer::getSkyboxTexture() const {
        return m_render_scene ? m_render_scene->getSkyboxTexture() : nullptr;
    }

    void Renderer::setSkyboxTexture(const gfx::TextureHandle& skybox_texture) {
        if (m_render_scene) {
            m_render_scene->setSkyboxTexture(skybox_texture);
        }
    }

} // dodoe
