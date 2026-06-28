// do@Redlive

#include "sky_light_system.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/renderer.h"
#include "runtime/function/render/render_scene/light_scene_info.h"
#include "runtime/function/render/framework/texture.h"
#include "runtime/resource/parser/texture_blob.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/core/utils/common.h"

namespace dodoe {

    SkyLightSystem::~SkyLightSystem() = default;

    void SkyLightSystem::update(Registry& reg, float dt) {
        (void)dt;

        auto view = reg.view<IDComponent, SkyLightComponent>();
        std::unordered_set<UUID> active{};
        bool dirty = false;

        for (auto entity : view) {
            auto& id = entity.getComponent<IDComponent>();
            auto& sky = entity.getComponent<SkyLightComponent>();
            active.insert(id.id);
            if (!sky.enabled) continue;
            dirty |= syncSkyLight(entity);
        }

        pruneRemoved(active);
        if (dirty) GetRenderSystem()->getRenderScene()->flushUpdates();
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
        data.cubemap = cubemap;
        data.intensity = sky.intensity;
        info.setSkyLightData(data);

        Renderer::AddLight(std::move(info));
        m_submitted.insert(id.id);

        id.dirty = false;
        sky.dirty = false;
        return true;
    }

    void SkyLightSystem::pruneRemoved(const std::unordered_set<UUID>& active) {
        for (auto it = m_submitted.begin(); it != m_submitted.end();) {
            if (!active.contains(*it)) {
                Renderer::RemoveLight(*it);
                it = m_submitted.erase(it);
            } else {
                ++it;
            }
        }
    }

    Ref<Texture> SkyLightSystem::loadCubemap(const DynamicArray<String>& paths) {
        auto& app = Application::Self();
        auto* rs = app.context().getRenderSystem();
        if (!rs || !rs->getGfx() || !rs->getGfx()->getDevice()) return nullptr;
        if (paths.size() < 6) return nullptr;

        const auto device = rs->getGfx()->getDevice();
        constexpr ui32 kFaceCount = 6;

        std::array<TextureBlob, kFaceCount> faces{};
        for (ui32 i = 0; i < kFaceCount; ++i) {
            auto fp = FileSystem::relative2absolute(paths[i]);
            faces[i].load(fp, false);
            if (!faces[i].isValid() || faces[i].width != faces[i].height) return nullptr;
        }

        auto desc = GfxTextureDesc()
            .setDimension(GfxTextureDimension::TextureCube)
            .setWidth(static_cast<UInt32>(faces[0].width))
            .setHeight(static_cast<UInt32>(faces[0].height))
            .setArraySize(kFaceCount)
            .setMipLevels(1)
            .setFormat(GfxFormat::RGBA32_FLOAT)
            .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
            .setDebugName("SkyLight Cubemap");
        auto cubemap = device->createTexture(desc);
        if (!cubemap) return nullptr;

        auto cmd = device->createCommandList();
        cmd->open();
        DynamicArray<float> top, bottom;
        for (ui32 i = 0; i < kFaceCount; ++i) {
            Size_t rp = static_cast<Size_t>(faces[i].width) * 4u * sizeof(Float);
            const void* px = faces[i].pixels;
            if (i == 2) { top = RotateCubemapFaceCW(static_cast<const float*>(faces[i].pixels), faces[i].width, faces[i].height); px = top.data(); }
            else if (i == 3) { bottom = RotateCubemapFaceCCW(static_cast<const float*>(faces[i].pixels), faces[i].width, faces[i].height); px = bottom.data(); }
            cmd->writeTexture(cubemap, i, 0, px, rp);
        }
        cmd->close();
        device->executeCommandList(cmd);

        auto texture = create_ref<Texture>();
        texture->setGpuHandle(cubemap);
        return texture;
    }

} // dodoe
