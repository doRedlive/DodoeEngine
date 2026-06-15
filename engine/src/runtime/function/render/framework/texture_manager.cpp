// do@Redlive

#include "texture_manager.h"

#include "texture.h"

#include "runtime/core/utils/common.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/resource/parser/texture_blob.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/renderer.h"
namespace dodoe {

    Ref<Texture> Texture::Load(const String& path) {
        auto* texture_manager = Renderer::GetTextureManager();
        if (!texture_manager) {
            return nullptr;
        }
        return texture_manager->loadTexture(path);
    }

    Bool TextureManager::initialize(const TextureManagerCreateInfo& info) {
        m_gfx = info.gfx;
        m_descriptor_table = info.descriptor_table;
        createFallbackTexture();
        return m_gfx && m_descriptor_table;
    }

    void TextureManager::shutdown() {
        m_texture_cache.clear();
        m_fallback = nullptr;
        m_descriptor_table = nullptr;
        m_gfx = nullptr;
    }

    Ref<Texture> TextureManager::loadTexture(const String& path) {
        const FileID file_id(path);
        const InstanceID existing = Object::FindInstanceID(file_id);
        if (existing != 0) {
            const auto it = m_texture_cache.find(existing);
            if (it != m_texture_cache.end()) {
                return it->second;
            }
        }

        return createTexture(path);
    }

    Ref<Texture> TextureManager::findTexture(const InstanceID id) {
        const auto it = m_texture_cache.find(id);
        if (it != m_texture_cache.end()) {
            return it->second;
        }
        return m_fallback;
    }

    Ref<Texture> TextureManager::getFallback() const {
        return m_fallback;
    }

    void TextureManager::removeTexture(const InstanceID id) {
        m_texture_cache.erase(id);
    }

    Ref<Texture> TextureManager::createTexture(const String& path) {
        TextureBlob data(path);
        if (!data.isValid()) {
            DO_ERROR("TextureManager: Create texture {} failed!", path);
            return nullptr;
        }

        const auto texture_format = data.is_hdr ? GfxFormat::RGBA32_FLOAT : GfxFormat::RGBA8_UNORM;
        const Size_t bytes_per_channel = data.is_hdr ? sizeof(Float) : sizeof(UByte);

        auto texture_desc = GfxTextureDesc()
            .setDimension(GfxTextureDimension::Texture2D)
            .setWidth(data.width)
            .setHeight(data.height)
            .setFormat(texture_format)
            .setMipLevels(1)
            .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
            .setDebugName(path);
        auto handle = m_gfx->getDevice()->createTexture(texture_desc);
        if (!handle) {
            DO_ERROR("TextureManager::createTexture: createTexture failed for {}.", path);
            return nullptr;
        }

        auto cmd = m_gfx->getCommandList();
        const Size_t row_pitch = static_cast<Size_t>(data.width) * 4u * bytes_per_channel;
        cmd->open();
        cmd->writeTexture(handle, 0, 0, data.pixels, row_pitch);
        cmd->close();
        m_gfx->getDevice()->executeCommandList(cmd);

        Ref<Texture> texture = create_ref<Texture>();
        texture->setDimensions(data.width, data.height);
        texture->setPath(path);
        texture->setGpuHandle(handle);
        texture->setDescriptorIndex(m_descriptor_table->createDescriptor(GfxBindingSetItem::Texture_SRV(0, texture->getGpuHandle())));

        const FileID file_id(path);
        texture->setFileIdentity(file_id, UUID{});
        Object::AllocateInstanceID(texture.get());

        m_texture_cache.emplace(texture->getInstanceID(), texture);
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
        auto handle = m_gfx->getDevice()->createTexture(texture_desc);

        auto upload_cmd = m_gfx->getCommandList();
        const UByte white[4] = {255, 255, 255, 255};
        upload_cmd->open();
        upload_cmd->writeTexture(handle, 0, 0, white, sizeof(white));
        upload_cmd->close();
        m_gfx->getDevice()->executeCommandList(upload_cmd);

        m_fallback = create_ref<Texture>();
        m_fallback->setName("<fallback>");
        m_fallback->setDimensions(1, 1);
        m_fallback->setGpuHandle(handle);
        m_fallback->setDescriptorIndex(m_descriptor_table->createDescriptor(GfxBindingSetItem::Texture_SRV(0, m_fallback->getGpuHandle())));
        m_fallback->setFileIdentity(FileID("<fallback>"), UUID(0));
        Object::AllocateInstanceID(m_fallback.get());
    }

} // dodoe
