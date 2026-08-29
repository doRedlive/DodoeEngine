// do@Redlive

#include "texture_manager.h"

#include "texture.h"

#include "runtime/core/utils/common.h"
#include "runtime/core/object/object_id.h"
#include "runtime/core/context/system_context.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/parser/texture_blob.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_command.h"
#include "runtime/function/render/render_settings.h"

namespace dodoe {

    namespace {

        ObjectID ResolveTextureRef(const String& path) {
            if (auto* asset_manager = ResourceManager::Self().getAssetManager()) {
                const ObjectID ref = asset_manager->resolvePathToRef(FileID(path));
                if (ref.isValid()) {
                    return ref;
                }
            }
            return ObjectID{UUID(static_cast<UInt64>(string2hash(path))), 0};
        }
    }

    TextureCubemap* TextureCubemap::LoadFromFaces(const DynamicArray<String>& face_paths) {
        auto* texture_manager = GetRenderSystem()->getSharedRenderService()->getTextureManager();
        if (!texture_manager) {
            return nullptr;
        }
        return texture_manager->loadCubemapTexture(face_paths);
    }

    Bool TextureManager::initialize(const TextureManagerCreateInfo& info) {
        m_gfx = info.gfx;
        m_descriptor_table = info.descriptor_table;
        m_device = m_gfx->getDevice();
        createFallbackTexture();
        return m_gfx != nullptr;
    }

    void TextureManager::shutdown() {
        m_slot_lut.clear();
        m_texture2d_cache.clear();
        m_cubemap_cache.clear();
        m_cubemap_by_path.clear();
        m_fallback = {};
        m_fallback_cubemap = {};
        m_device = nullptr;
        m_descriptor_table = nullptr;
        m_gfx = nullptr;
    }

    Texture* TextureManager::findTexture(const InstanceID id) {
        {
            const auto it = m_texture2d_cache.find(id);
            if (it != m_texture2d_cache.end()) {
                return it->second.get();
            }
        }
        {
            const auto it = m_cubemap_cache.find(id);
            if (it != m_cubemap_cache.end()) {
                return it->second.get();
            }
        }
        return m_fallback.get();
    }

    Texture2D* TextureManager::findTexture2D(const InstanceID id) {
        const auto it = m_texture2d_cache.find(id);
        if (it != m_texture2d_cache.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    Texture2D* TextureManager::getFallback() const {
        return m_fallback.get();
    }

    TextureCubemap* TextureManager::getFallbackCubemap() const {
        return m_fallback_cubemap.get();
    }

    void TextureManager::removeTexture(const InstanceID id) {
        m_texture2d_cache.erase(id);
        m_cubemap_cache.erase(id);
    }

    void TextureManager::realizeTexture(ResourceCommand& cmd) {
        DO_PROFILE_SCOPE_CATEGORY("TextureManager::realizeTexture", "render-command");
        auto texture = std::move(cmd.texture_object);
        if (!texture) { return; }

        const UInt32 width = static_cast<UInt32>(texture->getWidth());
        const UInt32 height = static_cast<UInt32>(texture->getHeight());

        auto texture_desc = GfxTextureDesc()
            .setDimension(GfxTextureDimension::Texture2D)
            .setWidth(width)
            .setHeight(height)
            .setFormat(cmd.texture_is_hdr ? GfxFormat::RGBA32_FLOAT : GfxFormat::RGBA8_UNORM)
            .setMipLevels(1)
            .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
            .setDebugName(texture->getPath().c_str());

        auto handle = create_ref<GfxTexture>(texture_desc);
        handle->initializeGpu(m_device);

        if (!cmd.resource_data.empty()) {
            const UInt32 bpp = cmd.texture_is_hdr ? 16u : 4u;
            const Size_t row_pitch = static_cast<Size_t>(width) * bpp;
            GDrawCommandList.writeTexture(handle, 0, 0, cmd.resource_data.data(), row_pitch);
        }
        texture->setGpuHandle(handle);

        const UInt32 slot = static_cast<UInt32>(m_slot_lut.size());
        m_slot_lut.push_back(texture->getInstanceID());
        texture->setSlot(slot);

        if (RenderSettings::IsBindlessActive()) {
            DescriptorIndex desc_idx = static_cast<DescriptorIndex>(m_descriptor_table->allocateSlot());
            DO_ASSERT(static_cast<UInt32>(desc_idx) == slot);
            auto item = GfxBindingSetItem::Texture_SRV(0, handle->getRHIHandle());
            item.slot = desc_idx;
            handle->getRHIHandle()->AddRef();
            m_device->writeDescriptorTable(m_descriptor_table->getDescriptorTable(), item);
            texture->setDescriptorIndex(desc_idx);
        }

        const InstanceID id = texture->getInstanceID();
        m_texture2d_cache.emplace(id, std::move(texture));
    }

    UInt32 TextureManager::resolveAtlasIndex(const Texture2D* texture) const {
        if (!texture) { return 0; }
        if (texture->getDescriptorIndex() >= 0) {
            return static_cast<UInt32>(texture->getDescriptorIndex());
        }
        return texture->getSlot();
    }

    void TextureManager::createFallbackTexture() {
        auto texture_desc = GfxTextureDesc()
            .setDimension(GfxTextureDimension::Texture2D)
            .setWidth(1)
            .setHeight(1)
            .setFormat(GfxFormat::RGBA8_UNORM)
            .setMipLevels(1)
            .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
            .setDebugName("Render TextureManager Fallback");

        const UByte white[4] = {255, 255, 255, 255};

        auto upload_cmd = m_device->createCommandList();
        upload_cmd->open();
        auto handle_rhi = m_device->createTexture(texture_desc);
        upload_cmd->writeTexture(handle_rhi, 0, 0, white, 4);
        upload_cmd->close();
        m_device->executeCommandList(upload_cmd);

        auto handle = create_ref<GfxTexture>(handle_rhi, texture_desc, "Render TextureManager Fallback");

        auto fb_scope = create_scope<Texture2D>(ObjectID{UUID(0), 1});
        Texture2D* fb = fb_scope.get();
        fb->setName("<fallback>");
        fb->setDimensions(1, 1);
        fb->setGpuHandle(handle);

        const UInt32 slot = static_cast<UInt32>(m_slot_lut.size());
        m_slot_lut.push_back(fb->getInstanceID());
        fb->setSlot(slot);

        if (RenderSettings::IsBindlessActive()) {
            auto fallback_item = GfxBindingSetItem::Texture_SRV(0, handle_rhi);
            DescriptorIndex fallback_descriptor_index = m_descriptor_table->createDescriptor(fallback_item);
            fb->setDescriptorIndex(fallback_descriptor_index);
        }

        m_fallback = std::move(fb_scope);

        const auto cube_desc = GfxTextureDesc()
            .setDimension(GfxTextureDimension::TextureCube)
            .setWidth(1)
            .setHeight(1)
            .setArraySize(6)
            .setMipLevels(1)
            .setFormat(GfxFormat::RGBA8_UNORM)
            .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
            .setDebugName("Render TextureManager Fallback Cubemap");

        const UByte black[4] = {0, 0, 0, 0};

        auto cube_upload = m_device->createCommandList();
        cube_upload->open();
        auto cube_rhi = m_device->createTexture(cube_desc);
        for (UInt32 face = 0; face < 6; ++face) {
            cube_upload->writeTexture(cube_rhi, face, 0, black, 4);
        }
        cube_upload->close();
        m_device->executeCommandList(cube_upload);

        auto cube_handle = create_ref<GfxTexture>(cube_rhi, cube_desc, "Render TextureManager Fallback Cubemap");
        auto cb_scope = create_scope<TextureCubemap>(ObjectID{UUID(0), 2});
        cb_scope->setFaceSize(1);
        cb_scope->setGpuHandle(cube_handle);
        m_fallback_cubemap = std::move(cb_scope);
    }

    TextureCubemap* TextureManager::loadCubemapTexture(const DynamicArray<String>& face_paths) {
        return loadCubemapTexture(face_paths, GDrawCommandList, nullptr);
    }

    TextureCubemap* TextureManager::loadCubemapTexture(const DynamicArray<String>& face_paths, DrawCommandList& cmd_list, FrameStagingAllocator* staging) {
        if (face_paths.size() < 6) return nullptr;

        const auto path_it = m_cubemap_by_path.find(face_paths[0]);
        if (path_it != m_cubemap_by_path.end()) {
            const InstanceID existing = path_it->second;
            const auto it = m_cubemap_cache.find(existing);
            if (it != m_cubemap_cache.end()) {
                return it->second.get();
            }
        }

        constexpr ui32 kFaceCount = 6;

        std::array<TextureBlob, kFaceCount> faces{};
        for (ui32 i = 0; i < kFaceCount; ++i) {
            auto fp = FileSystem::RelativeToAbsolute(face_paths[i], FileSystem::GetEngineResPath());
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
        auto cubemap = cmd_list.createTexture(desc);
        if (!cubemap) return nullptr;

        DynamicArray<float> top, bottom;
        for (ui32 i = 0; i < kFaceCount; ++i) {
            Size_t rp = static_cast<Size_t>(faces[i].width) * 4u * sizeof(Float);
            const void* px = faces[i].pixels;
            if (i == 2) { top = RotateCubemapFaceCW(static_cast<const float*>(faces[i].pixels), faces[i].width, faces[i].height); px = top.data(); }
            else if (i == 3) { bottom = RotateCubemapFaceCCW(static_cast<const float*>(faces[i].pixels), faces[i].width, faces[i].height); px = bottom.data(); }
            cmd_list.writeTexture(cubemap, i, 0, px, rp);
        }

        auto texture = create_scope<TextureCubemap>(ResolveTextureRef(face_paths[0]));
        texture->setFaceSize(faces[0].width);
        texture->setGpuHandle(cubemap);
        TextureCubemap* texture_raw = texture.get();

        const InstanceID id = texture->getInstanceID();
        m_cubemap_cache.emplace(id, std::move(texture));
        m_cubemap_by_path[face_paths[0]] = id;
        return texture_raw;
    }

    Texture2D* TextureManager::resolveSlot(const UInt32 slot) const {
        if (slot < m_slot_lut.size()) {
            if (auto* obj = Object::FindObjectFromInstanceID(m_slot_lut[slot])) {
                return static_cast<Texture2D*>(obj);
            }
        }
        return m_fallback.get();
    }

} // dodoe
