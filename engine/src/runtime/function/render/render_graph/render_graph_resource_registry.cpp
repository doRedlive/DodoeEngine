// do@Redlive

#include "render_graph_resource_registry.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    GfxTextureHandle TransientResourcePool::acquireTexture(const GfxTextureDesc& desc, DrawCommandList& command_list) {
        for (Size_t i = 0; i < m_textures.size(); i++) {
            if (!m_texture_in_use[i]) {
                const auto& pooled = m_textures[i].desc;
                if (pooled.width == desc.width && pooled.height == desc.height &&
                    pooled.format == desc.format && pooled.mipLevels == desc.mipLevels) {
                    m_texture_in_use[i] = true;
                    return m_textures[i].texture;
                }
            }
        }
        const auto texture = command_list.createTexture(desc);
        m_textures.push_back({texture, desc});
        m_texture_in_use.push_back(true);
        return texture;
    }

    GfxBufferHandle TransientResourcePool::acquireBuffer(const GfxBufferDesc& desc, DrawCommandList& command_list) {
        for (Size_t i = 0; i < m_buffers.size(); i++) {
            if (!m_buffer_in_use[i]) {
                const auto& pooled = m_buffers[i].desc;
                if (pooled.byteSize == desc.byteSize && pooled.format == desc.format) {
                    m_buffer_in_use[i] = true;
                    return m_buffers[i].buffer;
                }
            }
        }
        const auto buffer = command_list.createBuffer(desc);
        m_buffers.push_back({buffer, desc});
        m_buffer_in_use.push_back(true);
        return buffer;
    }

    void TransientResourcePool::releaseAll() {
        for (auto& in_use : m_texture_in_use) {
            in_use = false;
        }
        for (auto& in_use : m_buffer_in_use) {
            in_use = false;
        }
    }

    void TransientResourcePool::reset() {
        m_textures.clear();
        m_buffers.clear();
        m_texture_in_use.clear();
        m_buffer_in_use.clear();
    }

    RenderGraphResourceRegistry::RenderGraphResourceRegistry(
        const DynamicArray<RenderGraphResourceRecord>& resources,
        GfxContext& gfx_context,
        const UInt32 swapchain_image_index,
        DrawCommandList& command_list,
        TransientResourcePool* transient_pool)
    {
        m_transient_pool = transient_pool;
        reset();

        m_texture_handles.resize(resources.size());
        m_buffer_handles.resize(resources.size());

        for (Size_t resource_index = 0; resource_index < resources.size(); resource_index++) {
            const auto& resource = resources[resource_index];
            if (resource.type == RenderGraphResourceType::Texture) {
                switch (resource.source) {
                    case RenderGraphResourceSource::Transient:
                        if (m_transient_pool) {
                            m_texture_handles[resource_index] = m_transient_pool->acquireTexture(resource.texture_desc.desc, command_list);
                        } else {
                            m_texture_handles[resource_index] = command_list.createTexture(resource.texture_desc.desc);
                        }
                        break;
                    case RenderGraphResourceSource::ImportedTexture:
                        DO_ASSERT(resource.imported_texture != nullptr, "RenderGraphResourceRegistry imported texture is null");
                        m_texture_handles[resource_index] = resource.imported_texture;
                        break;
                    case RenderGraphResourceSource::ImportedBackBuffer: {
                        const auto& swapchain_textures = gfx_context.getSwapchainTextures();
                        DO_ASSERT(swapchain_image_index < swapchain_textures.size(), "RenderGraphResourceRegistry swapchain image index out of range");
                        m_texture_handles[resource_index] = swapchain_textures[swapchain_image_index];
                        break;
                    }
                    case RenderGraphResourceSource::ImportedBuffer:
                        DO_ASSERT(false, "RenderGraphResourceRegistry texture resource cannot use imported buffer source");
                        break;
                }
                continue;
            }

            switch (resource.source) {
                case RenderGraphResourceSource::Transient:
                    m_buffer_handles[resource_index] = command_list.createBuffer(resource.buffer_desc.desc);
                    break;
                case RenderGraphResourceSource::ImportedBuffer:
                    DO_ASSERT(resource.imported_buffer != nullptr, "RenderGraphResourceRegistry imported buffer is null");
                    m_buffer_handles[resource_index] = resource.imported_buffer;
                    break;
                case RenderGraphResourceSource::ImportedTexture:
                case RenderGraphResourceSource::ImportedBackBuffer:
                    DO_ASSERT(false, "RenderGraphResourceRegistry buffer resource cannot use imported texture source");
                    break;
            }
        }
    }

    void RenderGraphResourceRegistry::reset() {
        m_texture_handles.clear();
        m_buffer_handles.clear();
    }

    GfxTextureHandle RenderGraphResourceRegistry::getTexture(const RenderGraphTextureHandle handle) const {
        DO_ASSERT(handle.isValid(), "RenderGraphResourceRegistry invalid texture handle");
        DO_ASSERT(handle.index < m_texture_handles.size(), "RenderGraphResourceRegistry texture handle out of range");
        return m_texture_handles[handle.index];
    }

    GfxBufferHandle RenderGraphResourceRegistry::getBuffer(const RenderGraphBufferHandle handle) const {
        DO_ASSERT(handle.isValid(), "RenderGraphResourceRegistry invalid buffer handle");
        DO_ASSERT(handle.index < m_buffer_handles.size(), "RenderGraphResourceRegistry buffer handle out of range");
        return m_buffer_handles[handle.index];
    }

} // dodoe