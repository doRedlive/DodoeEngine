// do@Redlive

#include "render_graph_resource_registry.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    RenderGraphResourceRegistry::RenderGraphResourceRegistry(
        const DynamicArray<RenderGraphResourceRecord>& resources,
        GfxContext& gfx_context,
        const UInt32 swapchain_image_index,
        DrawCommandList& command_list,
        RenderGraphTransientPool* transient_pool)
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
                    m_buffer_handles[resource_index] = m_transient_pool->acquireBuffer(resource.buffer_desc.desc, command_list);
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