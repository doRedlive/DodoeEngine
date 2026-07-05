// do@Redlive

#include "renderer.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_command.h"
#include "runtime/function/render/render_settings.h"
#include "render_view/render_view.h"

namespace dodoe {

    void Renderer::AddPrimitive(Scope<PrimitiveRenderObject> primitive) {
        auto* rs = GetRenderSystem();
        if (RenderSettings::GetThreadingMode() == ThreadingMode::SingleThread) {
            rs->getRenderScene()->addPrimitive(std::move(primitive));
        } else {
            RenderCommand cmd;
            cmd.type = RenderCommandType::AddPrimitive;
            cmd.primitive = std::move(primitive);
            rs->enqueueRenderCommand(std::move(cmd));
        }
    }

    void Renderer::RemovePrimitive(UUID id) {
        auto* rs = GetRenderSystem();
        if (RenderSettings::GetThreadingMode() == ThreadingMode::SingleThread) {
            rs->getRenderScene()->removePrimitive(id);
        } else {
            RenderCommand cmd;
            cmd.type = RenderCommandType::RemovePrimitive;
            cmd.id = id;
            rs->enqueueRenderCommand(std::move(cmd));
        }
    }

    void Renderer::UpdatePrimitiveTransform(UUID id, const Matrix4f& world_transform) {
        auto* rs = GetRenderSystem();
        if (RenderSettings::GetThreadingMode() == ThreadingMode::SingleThread) {
            rs->getRenderScene()->updatePrimitiveTransform(id, world_transform);
        } else {
            RenderCommand cmd;
            cmd.type = RenderCommandType::UpdatePrimitiveTransform;
            cmd.id = id;
            cmd.transform = world_transform;
            rs->enqueueRenderCommand(std::move(cmd));
        }
    }

    void Renderer::AddLight(LightSceneInfo&& info) {
        auto* rs = GetRenderSystem();
        if (RenderSettings::GetThreadingMode() == ThreadingMode::SingleThread) {
            rs->getRenderScene()->addLightSceneInfo(std::move(info));
        } else {
            RenderCommand cmd;
            cmd.type = RenderCommandType::AddLight;
            cmd.light = std::move(info);
            rs->enqueueRenderCommand(std::move(cmd));
        }
    }

    void Renderer::RemoveLight(UUID id) {
        auto* rs = GetRenderSystem();
        if (RenderSettings::GetThreadingMode() == ThreadingMode::SingleThread) {
            rs->getRenderScene()->removeLightSceneInfo(id);
        } else {
            RenderCommand cmd;
            cmd.type = RenderCommandType::RemoveLight;
            cmd.id = id;
            rs->enqueueRenderCommand(std::move(cmd));
        }
    }

    void Renderer::UpdateLightTransform(UUID id, const Matrix4f& world_transform) {
        auto* rs = GetRenderSystem();
        if (RenderSettings::GetThreadingMode() == ThreadingMode::SingleThread) {
            rs->getRenderScene()->updateLightSceneInfoTransform(id, world_transform);
        } else {
            RenderCommand cmd;
            cmd.type = RenderCommandType::UpdateLightTransform;
            cmd.id = id;
            cmd.transform = world_transform;
            rs->enqueueRenderCommand(std::move(cmd));
        }
    }

    void Renderer::AddSprite(Scope<SpriteRenderObject> sprite) {
        auto* rs = GetRenderSystem();
        DO_DEBUG("Renderer::AddSprite: id={}, threading_mode={}",
                  static_cast<UInt64>(sprite ? sprite->getUUID() : UUID(0)),
                  static_cast<int>(RenderSettings::GetThreadingMode()));
        if (RenderSettings::GetThreadingMode() == ThreadingMode::SingleThread) {
            rs->getRenderScene()->addSprite(std::move(sprite));
        } else {
            RenderCommand cmd;
            cmd.type = RenderCommandType::AddSprite;
            cmd.sprite = std::move(sprite);
            rs->enqueueRenderCommand(std::move(cmd));
        }
    }

    void Renderer::RemoveSprite(UUID id) {
        auto* rs = GetRenderSystem();
        if (RenderSettings::GetThreadingMode() == ThreadingMode::SingleThread) {
            rs->getRenderScene()->removeSprite(id);
        } else {
            RenderCommand cmd;
            cmd.type = RenderCommandType::RemoveSprite;
            cmd.id = id;
            rs->enqueueRenderCommand(std::move(cmd));
        }
    }

    void Renderer::UpdateSpriteTransform(UUID id, const Matrix4f& world_transform) {
        auto* rs = GetRenderSystem();
        if (RenderSettings::GetThreadingMode() == ThreadingMode::SingleThread) {
            rs->getRenderScene()->updateSpriteTransform(id, world_transform);
        } else {
            RenderCommand cmd;
            cmd.type = RenderCommandType::UpdateSpriteTransform;
            cmd.id = id;
            cmd.transform = world_transform;
            rs->enqueueRenderCommand(std::move(cmd));
        }
    }

} // dodoe
