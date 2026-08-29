// do@Redlive

#include "render_command_queue.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_command.h"
#include "runtime/function/render/render_settings.h"
#include "render_view/render_view.h"

namespace dodoe {

    namespace {

        RenderSystem* requireRenderSystem() {
            return GetRenderSystem();
        }

        void enqueueResourceCommand(ResourceCommand&& cmd) {
            auto* rs = requireRenderSystem();
            if (!rs) { return; }
            if (RenderSettings::IsSingleThread()) {
                rs->realizeResourceCommand(cmd);
            } else {
                rs->enqueueResourceCommand(std::move(cmd));
            }
        }

        void enqueueSceneCommand(SceneCommand&& cmd) {
            auto* rs = requireRenderSystem();
            if (!rs) { return; }
            if (RenderSettings::IsSingleThread()) {
                rs->applySceneCommand(*rs->getRenderScene(), cmd);
            } else {
                rs->enqueueSceneCommand(std::move(cmd));
            }
        }
    }

    GfxBufferHandle RenderResourceQueue::CreateBuffer(const GfxBufferDesc& desc, const void* data, Size_t data_size) {
        auto* rs = requireRenderSystem();
        if (!rs) { return {}; }
        auto buffer = create_ref<GfxBuffer>(desc);
        ResourceCommand cmd;
        cmd.type = ResourceCommandType::CreateBuffer;
        cmd.buffer = buffer;
        cmd.buffer_desc = desc;
        if (data && data_size > 0) {
            cmd.resource_data.assign(static_cast<const UInt8*>(data), static_cast<const UInt8*>(data) + data_size);
        }
        enqueueResourceCommand(std::move(cmd));
        return buffer;
    }

    void RenderCommandQueue::AddPrimitive(Scope<PrimitiveRenderObject> primitive) {
        SceneCommand cmd;
        cmd.type = SceneCommandType::AddPrimitive;
        cmd.primitive = std::move(primitive);
        enqueueSceneCommand(std::move(cmd));
    }

    void RenderCommandQueue::RemovePrimitive(UUID id) {
        SceneCommand cmd;
        cmd.type = SceneCommandType::RemovePrimitive;
        cmd.id = id;
        enqueueSceneCommand(std::move(cmd));
    }

    void RenderCommandQueue::UpdatePrimitiveTransform(UUID id, const Matrix4f& world_transform) {
        SceneCommand cmd;
        cmd.type = SceneCommandType::UpdatePrimitiveTransform;
        cmd.id = id;
        cmd.transform = world_transform;
        enqueueSceneCommand(std::move(cmd));
    }

    void RenderCommandQueue::AddLight(LightSceneInfo&& info) {
        SceneCommand cmd;
        cmd.type = SceneCommandType::AddLight;
        cmd.light = std::move(info);
        enqueueSceneCommand(std::move(cmd));
    }

    void RenderCommandQueue::RemoveLight(UUID id) {
        SceneCommand cmd;
        cmd.type = SceneCommandType::RemoveLight;
        cmd.id = id;
        enqueueSceneCommand(std::move(cmd));
    }

    void RenderCommandQueue::UpdateLightTransform(UUID id, const Matrix4f& world_transform) {
        SceneCommand cmd;
        cmd.type = SceneCommandType::UpdateLightTransform;
        cmd.id = id;
        cmd.transform = world_transform;
        enqueueSceneCommand(std::move(cmd));
    }

    void RenderCommandQueue::AddSprite(Scope<SpriteRenderObject> sprite) {
        SceneCommand cmd;
        cmd.type = SceneCommandType::AddSprite;
        cmd.sprite = std::move(sprite);
        enqueueSceneCommand(std::move(cmd));
    }

    void RenderCommandQueue::RemoveSprite(UUID id) {
        SceneCommand cmd;
        cmd.type = SceneCommandType::RemoveSprite;
        cmd.id = id;
        enqueueSceneCommand(std::move(cmd));
    }

    void RenderCommandQueue::UpdateSpriteTransform(UUID id, const Matrix4f& world_transform) {
        SceneCommand cmd;
        cmd.type = SceneCommandType::UpdateSpriteTransform;
        cmd.id = id;
        cmd.transform = world_transform;
        enqueueSceneCommand(std::move(cmd));
    }

    void RenderCommandQueue::SubmitUI(DynamicArray<UISceneInfo> instances) {
        SceneCommand cmd;
        cmd.type = SceneCommandType::SubmitUIBatch;
        cmd.ui_scene_infos = std::move(instances);
        enqueueSceneCommand(std::move(cmd));
    }

} // dodoe
