// do@Redlive

#include "renderer.h"

#include "runtime/core/context/system_context.h"
#include "render_view/render_view.h"

namespace dodoe {

    void Renderer::AddPrimitive(Scope<PrimitiveRenderObject> primitive) {
        auto* rs = GetRenderSystem();
        if (rs && rs->getRenderScene()) rs->getRenderScene()->addPrimitive(std::move(primitive));
    }

    void Renderer::RemovePrimitive(UUID id) {
        auto* rs = GetRenderSystem();
        if (rs && rs->getRenderScene()) rs->getRenderScene()->removePrimitive(id);
    }

    void Renderer::UpdatePrimitiveTransform(UUID id, const Matrix4f& world_transform) {
        auto* rs = GetRenderSystem();
        if (rs && rs->getRenderScene()) rs->getRenderScene()->updatePrimitiveTransform(id, world_transform);
    }

    void Renderer::AddLight(LightSceneInfo&& info) {
        auto* rs = GetRenderSystem();
        if (rs && rs->getRenderScene()) rs->getRenderScene()->addLightSceneInfo(std::move(info));
    }

    void Renderer::RemoveLight(UUID id) {
        auto* rs = GetRenderSystem();
        if (rs && rs->getRenderScene()) rs->getRenderScene()->removeLightSceneInfo(id);
    }

    void Renderer::UpdateLightTransform(UUID id, const Matrix4f& world_transform) {
        auto* rs = GetRenderSystem();
        if (rs && rs->getRenderScene()) rs->getRenderScene()->updateLightSceneInfoTransform(id, world_transform);
    }

    void Renderer::AddSprite(Scope<SpriteRenderObject> sprite) {
        auto* rs = GetRenderSystem();
        if (rs && rs->getRenderScene()) rs->getRenderScene()->addSprite(std::move(sprite));
    }

    void Renderer::RemoveSprite(UUID id) {
        auto* rs = GetRenderSystem();
        if (rs && rs->getRenderScene()) rs->getRenderScene()->removeSprite(id);
    }

    void Renderer::UpdateSpriteTransform(UUID id, const Matrix4f& world_transform) {
        auto* rs = GetRenderSystem();
        if (rs && rs->getRenderScene()) rs->getRenderScene()->updateSpriteTransform(id, world_transform);
    }

} // dodoe
