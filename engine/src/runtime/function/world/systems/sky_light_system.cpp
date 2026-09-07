// do@Redlive

#include "sky_light_system.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/project/project.h"
#include "runtime/function/render/render_command_queue.h"
#include "runtime/function/render/render_pipeline/renderer.h"
#include "runtime/function/render/render_scene/light_scene_info.h"
#include "runtime/function/render/texture/texture_manager.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/resource/resource_manager.h"

namespace dodoe {

    SkyLightSystem::~SkyLightSystem() = default;

    SystemAccess SkyLightSystem::getAccess() const {
        return SystemAccessBuilder{}
            .readsComponents<IDComponent, SkyLightComponent, ActiveComponent, HierarchyComponent>()
            .build();
    }

    void SkyLightSystem::update(Registry& reg, float dt) {
        (void)dt;
        if (!GetRenderSystem()) { return; }

        auto view = reg.view<IDComponent, SkyLightComponent>();
        UnorderedSet<UUID> active{};

        for (auto entity : view) {
            if (!entity.activeInHierarchy()) {
                static Bool logged_inactive = false;
                if (!logged_inactive) {
                    logged_inactive = true;
                    DO_WARN("SkyLightSystem: skylight entity inactive in hierarchy, skipped");
                }
                continue;
            }
            auto& id = entity.getComponent<IDComponent>();
            auto& sky = entity.getComponent<SkyLightComponent>();
            active.insert(id.id);
            if (!sky.enabled) {
                static Bool logged_disabled = false;
                if (!logged_disabled) {
                    logged_disabled = true;
                    DO_WARN("SkyLightSystem: skylight '{}' disabled, skipped", static_cast<UInt64>(id.id));
                }
                continue;
            }
            syncSkyLight(entity);
        }

        pruneRemoved(active);
    }

    bool SkyLightSystem::syncSkyLight(Entity entity) {
        auto& id = entity.getComponent<IDComponent>();
        auto& sky = entity.getComponent<SkyLightComponent>();

        if (!sky.dirty) {
            static Bool logged_not_dirty = false;
            if (!logged_not_dirty) {
                logged_not_dirty = true;
                DO_WARN("SkyLightSystem: sync skipped, entity {} cubemap not dirty", static_cast<UInt64>(id.id));
            }
            return false;
        }

        TextureCubemap* cubemap = sky.cubemap.get();
        if (!cubemap && sky.cubemap.getObjectID().isValid()) {
            const ObjectID& ref = sky.cubemap.getObjectID();
            cubemap = ResourceManager::Self().loadObject<TextureCubemap>(ref.asset_id, ref.local_id);
        }
        if (!cubemap && !sky.cubemap.getLegacyPath().empty()) {
            cubemap = ResourceManager::Self().loadObjectByPath<TextureCubemap>(FileID(sky.cubemap.getLegacyPath()));
        }
        if (!cubemap) {
            static Bool logged_no_cubemap = false;
            if (!logged_no_cubemap) {
                logged_no_cubemap = true;
                DO_WARN("SkyLightSystem: failed to load cubemap (asset_id_valid={} legacy_path='{}')",
                    sky.cubemap.getObjectID().isValid(), sky.cubemap.getLegacyPath());
            }
            return false;
        }

        const String legacy_path = sky.cubemap.getLegacyPath();
        sky.cubemap = PPtr<TextureCubemap>(cubemap);
        if (!legacy_path.empty()) {
            sky.cubemap.setLegacyPath(legacy_path);
        }

        if (cubemap->getFaceSize() == 0) {
            if (!loadCubemap(cubemap)) {
                static Bool logged_load_failed = false;
                if (!logged_load_failed) {
                    logged_load_failed = true;
                    DO_WARN("SkyLightSystem: cubemap GPU load failed for '{}'", cubemap->getPath());
                }
                return false;
            }
        }

        LightSceneInfo info(RenderId(static_cast<UInt64>(id.id)));
        info.setLightType(LightType::Sky);
        info.setWorldTransform(Matrix4f(1.0f));
        info.setEnabled(sky.enabled);

        SkyLightData data{};
        data.cubemap = cubemap;
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

    bool SkyLightSystem::loadCubemap(TextureCubemap* cubemap) {
        auto* tm = GetRenderSystem()->getSharedRenderService()->getTextureManager();
        if (!tm) {
            DO_WARN("SkyLightSystem: texture manager unavailable");
            return false;
        }

        DynamicArray<String> resolved_paths = cubemap->getFacePaths();
        const FsPath asset_dir = Project::AssetDirectory();
        for (auto& path : resolved_paths) {
            if (path.empty()) {
                continue;
            }
            const FsPath candidate = asset_dir / FsPath(path.c_str());
            if (FileSystem::IsFileExists(candidate)) {
                path = String(candidate.lexically_normal().generic_string().c_str());
            }
        }

        TextureCubemap* loaded = tm->loadCubemapTexture(resolved_paths);
        if (!loaded) {
            DO_WARN("SkyLightSystem: loadCubemapTexture failed for face0='{}'", resolved_paths.empty() ? "" : resolved_paths[0]);
            return false;
        }
        cubemap->setGpuHandle(loaded->getGpuHandle());
        cubemap->setFaceSize(loaded->getFaceSize());
        cubemap->setIrradianceSH(loaded->getIrradianceSH());
        return true;
    }
} // dodoe
