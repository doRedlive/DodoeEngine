// do@Redlive

#include "sky_light_system.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_command_queue.h"
#include "runtime/function/render/render_pipeline/renderer.h"
#include "runtime/function/render/render_scene/light_scene_info.h"
#include "runtime/function/render/texture/texture.h"

namespace dodoe {

    SkyLightSystem::~SkyLightSystem() = default;

    void SkyLightSystem::update(Registry& reg, float dt) {
        (void)dt;

        auto view = reg.view<IDComponent, SkyLightComponent>();
        UnorderedSet<UUID> active{};

        for (auto entity : view) {
            auto& id = entity.getComponent<IDComponent>();
            auto& sky = entity.getComponent<SkyLightComponent>();
            active.insert(id.id);
            if (!sky.enabled) continue;
            syncSkyLight(entity);
        }

        pruneRemoved(active);
    }

    bool SkyLightSystem::syncSkyLight(Entity entity) {
        auto& id = entity.getComponent<IDComponent>();
        auto& sky = entity.getComponent<SkyLightComponent>();

        if (!sky.dirty) return false;

        auto cubemap = loadCubemap(sky.face_paths);
        if (!cubemap) return false;

        LightSceneInfo info(static_cast<Identifier>(static_cast<uint64_t>(id.id)));
        info.setLightType(LightType::Sky);
        info.setWorldTransform(Matrix4f(1.0f));
        info.setEnabled(sky.enabled);

        SkyLightData data{};
        data.cubemap = ObjHandle<TextureCubemap>(cubemap);
        data.intensity = sky.intensity;
        info.setSkyLightData(data);

        RenderCommandQueue::AddLight(std::move(info));
        m_submitted.insert(id.id);

        id.dirty = false;
        sky.dirty = false;
        return true;
    }

    void SkyLightSystem::pruneRemoved(const UnorderedSet<UUID>& active) {
        for (auto it = m_submitted.begin(); it != m_submitted.end();) {
            if (!active.contains(*it)) {
                RenderCommandQueue::RemoveLight(*it);
                it = m_submitted.erase(it);
            } else {
                ++it;
            }
        }
    }

    TextureCubemap* SkyLightSystem::loadCubemap(const DynamicArray<String>& paths) {
        auto* tm = GetRenderSystem()->getSharedRenderService()->getTextureManager();
        if (!tm) return nullptr;
        return tm->loadCubemapTexture(paths);
    }

} // dodoe
