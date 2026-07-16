// do@Redlive

#include "texture_manager.h"

#include "texture.h"

#include "runtime/core/utils/common.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/resource/parser/texture_blob.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/gc/object_heap.h"

namespace dodoe {

    Texture2D* Texture2D::Load(const String& path) {
        auto* texture_manager = GetRenderSystem()->getTextureManager();
        if (!texture_manager) {
            return nullptr;
        }
        return texture_manager->loadTexture(path);
    }

    TextureCubemap* TextureCubemap::LoadFromFaces(const DynamicArray<String>& face_paths) {
        auto* texture_manager = GetRenderSystem()->getTextureManager();
        if (!texture_manager) {
            return nullptr;
        }
        return texture_manager->loadCubemapTexture(face_paths);
    }

    Bool TextureManager::initialize(const TextureManagerCreateInfo& info) {
        m_gfx = info.gfx;
        m_descriptor_table = info.descriptor_table;
        createFallbackTexture();
        return m_gfx && m_descriptor_table;
    }

    void TextureManager::shutdown() {
        m_texture2d_cache.clear();
        m_cubemap_cache.clear();
        m_fallback = {};
        m_descriptor_table = nullptr;
        m_gfx = nullptr;
    }

    Texture2D* TextureManager::loadTexture(const String& path) {
        return loadTexture(path, GDrawCommandList);
    }

    Texture2D* TextureManager::loadTexture(const String& path, DrawCommandList& cmd_list) {
        const FileID file_id(path);
        const InstanceID existing = Object::FindInstanceID(file_id);
        if (existing != 0) {
            const auto it = m_texture2d_cache.find(existing);
            if (it != m_texture2d_cache.end()) {
                return it->second.get();
            }
        }

        return createTexture(path, cmd_list);
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

    void TextureManager::removeTexture(const InstanceID id) {
        m_texture2d_cache.erase(id);
        m_cubemap_cache.erase(id);
    }

    Texture2D* TextureManager::createTexture(const String& path, DrawCommandList& cmd_list) {
        TextureBlob data(path);
        if (!data.isValid()) {
            DO_ERROR("TextureManager: Create texture {} failed!", path);
            return nullptr;
        }

        const auto texture_format = data.is_hdr ? GfxFormat::RGBA32_FLOAT : GfxFormat::RGBA8_UNORM;
        const Size_t bytes_per_channel = data.is_hdr ? sizeof(Float) : sizeof(UByte);
        const Size_t data_size = static_cast<Size_t>(data.width) * static_cast<Size_t>(data.height) * 4u * bytes_per_channel;

        auto texture_desc = GfxTextureDesc()
            .setDimension(GfxTextureDimension::Texture2D)
            .setWidth(data.width)
            .setHeight(data.height)
            .setFormat(texture_format)
            .setMipLevels(1)
            .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
            .setDebugName(path);

        const FileID file_id(path);
        auto* texture = ObjectHeap::Construct<Texture2D>(AllocCategory::Texture, file_id);
        texture->setDimensions(data.width, data.height);
        texture->setPath(path);

        auto handle = cmd_list.createTexture(texture_desc);
        if (data.pixels && data_size > 0) {
            const UInt32 bytes_per_pixel = data.is_hdr ? 16u : 4u;
            const Size_t row_pitch = static_cast<Size_t>(data.width) * bytes_per_pixel;
            cmd_list.writeTexture(handle, 0, 0, data.pixels, row_pitch);
        }
        texture->setGpuHandle(handle);

        auto item = GfxBindingSetItem::Texture_SRV(0, handle->getRHIHandle());
        DescriptorIndex descriptor_index = m_descriptor_table->createDescriptor(item);
        const UInt32 slot = static_cast<UInt32>(descriptor_index);

        texture->setDescriptorIndex(static_cast<DescriptorIndex>(slot));

        m_texture2d_cache.emplace(texture->getInstanceID(), ObjHandle<Texture2D>(texture));
        return texture;
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

        auto handle = GDrawCommandList.createTexture(texture_desc);

        const UByte white[4] = {255, 255, 255, 255};
        GDrawCommandList.writeTexture(handle, 0, 0, white, 4);

        const auto device = GDrawCommandList.getDevice();
        auto upload_cmd = device->createCommandList();
        upload_cmd->open();
        GDrawCommandList.execute(upload_cmd);
        GDrawCommandList.reset();
        upload_cmd->close();
        device->executeCommandList(upload_cmd);

        auto* fb = ObjectHeap::Construct<Texture2D>(AllocCategory::Texture, FileID("<fallback>"), UUID(0));
        fb->setName("<fallback>");
        fb->setDimensions(1, 1);
        fb->setGpuHandle(handle);
        auto fallback_item = GfxBindingSetItem::Texture_SRV(0, handle->getRHIHandle());
        DescriptorIndex fallback_descriptor_index = m_descriptor_table->createDescriptor(fallback_item);
        fb->setDescriptorIndex(fallback_descriptor_index);

        m_fallback = ObjHandle<Texture2D>(fb);
    }

    TextureCubemap* TextureManager::loadCubemapTexture(const DynamicArray<String>& face_paths) {
        if (face_paths.size() < 6) return nullptr;

        const FileID file_id(face_paths[0]);
        const InstanceID existing = Object::FindInstanceID(file_id);
        if (existing != 0) {
            const auto it = m_cubemap_cache.find(existing);
            if (it != m_cubemap_cache.end()) {
                return it->second.get();
            }
        }

        const auto device = GDrawCommandList.getDevice();
        constexpr ui32 kFaceCount = 6;

        std::array<TextureBlob, kFaceCount> faces{};
        for (ui32 i = 0; i < kFaceCount; ++i) {
            auto fp = FileSystem::relative2absolute(face_paths[i]);
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
        auto cubemap = GDrawCommandList.createTexture(desc);
        if (!cubemap) return nullptr;

        auto cmd = device->createCommandList();
        cmd->open();
        DynamicArray<float> top, bottom;
        for (ui32 i = 0; i < kFaceCount; ++i) {
            Size_t rp = static_cast<Size_t>(faces[i].width) * 4u * sizeof(Float);
            const void* px = faces[i].pixels;
            if (i == 2) { top = RotateCubemapFaceCW(static_cast<const float*>(faces[i].pixels), faces[i].width, faces[i].height); px = top.data(); }
            else if (i == 3) { bottom = RotateCubemapFaceCCW(static_cast<const float*>(faces[i].pixels), faces[i].width, faces[i].height); px = bottom.data(); }
            cmd->writeTexture(cubemap->getRHIHandle(), i, 0, px, rp);
        }
        cmd->close();
        device->executeCommandList(cmd);

        auto* texture = ObjectHeap::Construct<TextureCubemap>(AllocCategory::Texture, file_id);
        texture->setFaceSize(faces[0].width);
        texture->setGpuHandle(cubemap);

        m_cubemap_cache.emplace(texture->getInstanceID(), ObjHandle<TextureCubemap>(texture));
        return texture;
    }

} // dodoe
