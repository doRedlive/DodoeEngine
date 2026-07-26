// do@Redlive

#include "render_graph_resource_resolver.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    RenderGraphResourceResolver::RenderGraphResourceResolver(
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
                        DO_ASSERT(resource.imported_texture != nullptr, "RenderGraphResourceResolver imported texture is null");
                        m_texture_handles[resource_index] = resource.imported_texture;
                        break;
                    case RenderGraphResourceSource::ImportedBackBuffer: {
                        const auto& swapchain_textures = gfx_context.getSwapchainTextures();
                        DO_ASSERT(swapchain_image_index < swapchain_textures.size(), "RenderGraphResourceResolver swapchain image index out of range");
                        m_texture_handles[resource_index] = swapchain_textures[swapchain_image_index];
                        break;
                    }
                    case RenderGraphResourceSource::ImportedBuffer:
                        DO_ASSERT(false, "RenderGraphResourceResolver texture resource cannot use imported buffer source");
                        break;
                }
                continue;
            }

            switch (resource.source) {
                case RenderGraphResourceSource::Transient:
                    m_buffer_handles[resource_index] = m_transient_pool->acquireBuffer(resource.buffer_desc.desc, command_list);
                    break;
                case RenderGraphResourceSource::ImportedBuffer:
                    DO_ASSERT(resource.imported_buffer != nullptr, "RenderGraphResourceResolver imported buffer is null");
                    m_buffer_handles[resource_index] = resource.imported_buffer;
                    break;
                case RenderGraphResourceSource::ImportedTexture:
                case RenderGraphResourceSource::ImportedBackBuffer:
                    DO_ASSERT(false, "RenderGraphResourceResolver buffer resource cannot use imported texture source");
                    break;
            }
        }
    }

    void RenderGraphResourceResolver::reset() {
        m_texture_handles.clear();
        m_buffer_handles.clear();
    }

    GfxTextureHandle RenderGraphResourceResolver::getTexture(const RenderGraphTextureHandle handle) const {
        DO_ASSERT(handle.isValid(), "RenderGraphResourceResolver invalid texture handle");
        DO_ASSERT(handle.index < m_texture_handles.size(), "RenderGraphResourceResolver texture handle out of range");
        return m_texture_handles[handle.index];
    }

    GfxBufferHandle RenderGraphResourceResolver::getBuffer(const RenderGraphBufferHandle handle) const {
        DO_ASSERT(handle.isValid(), "RenderGraphResourceResolver invalid buffer handle");
        DO_ASSERT(handle.index < m_buffer_handles.size(), "RenderGraphResourceResolver buffer handle out of range");
        return m_buffer_handles[handle.index];
    }

} // dodoe
