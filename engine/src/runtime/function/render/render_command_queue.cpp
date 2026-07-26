// do@Redlive

#include "render_command_queue.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_command.h"
#include "runtime/function/render/render_settings.h"
#include "render_view/render_view.h"

namespace dodoe {

    void RenderCommandQueue::AddPrimitive(Scope<PrimitiveRenderObject> primitive) {
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

    void RenderCommandQueue::RemovePrimitive(UUID id) {
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

    void RenderCommandQueue::UpdatePrimitiveTransform(UUID id, const Matrix4f& world_transform) {
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

    void RenderCommandQueue::AddLight(LightSceneInfo&& info) {
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

    void RenderCommandQueue::RemoveLight(UUID id) {
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

    void RenderCommandQueue::UpdateLightTransform(UUID id, const Matrix4f& world_transform) {
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

    void RenderCommandQueue::AddSprite(Scope<SpriteRenderObject> sprite) {
        auto* rs = GetRenderSystem();
        if (RenderSettings::GetThreadingMode() == ThreadingMode::SingleThread) {
            rs->getRenderScene()->addSprite(std::move(sprite));
        } else {
            RenderCommand cmd;
            cmd.type = RenderCommandType::AddSprite;
            cmd.sprite = std::move(sprite);
            rs->enqueueRenderCommand(std::move(cmd));
        }
    }

    void RenderCommandQueue::RemoveSprite(UUID id) {
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

    void RenderCommandQueue::UpdateSpriteTransform(UUID id, const Matrix4f& world_transform) {
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
