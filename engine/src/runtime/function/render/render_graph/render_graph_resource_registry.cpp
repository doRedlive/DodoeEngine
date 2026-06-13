// do@Redlive

#include "render_graph_resource_registry.h"

namespace dodoe {

    void RenderGraphResourceRegistry::initialize(
        const DynamicArray<RenderGraphResourceRecord>& resources,
        GfxContext& gfx_context,
        const UInt32 swapchain_image_index)
    {
        reset();

        const auto device = gfx_context.getDevice();
        DO_ASSERT(device != nullptr, "RenderGraphResourceRegistry device is null");

        m_texture_handles.resize(resources.size());
        m_buffer_handles.resize(resources.size());

        for (Size_t resource_index = 0; resource_index < resources.size(); resource_index++) {
            const auto& resource = resources[resource_index];
            if (resource.type == RenderGraphResourceType::Texture) {
                m_texture_handles[resource_index] = device->createTexture(resource.texture_desc.desc);
                continue;
            }

            m_buffer_handles[resource_index] = device->createBuffer(resource.buffer_desc.desc);
        }

        const auto& swapchain_textures = gfx_context.getSwapchainTextures();
        DO_ASSERT(swapchain_image_index < swapchain_textures.size(), "RenderGraphResourceRegistry swapchain image index out of range");
        m_backbuffer = swapchain_textures[swapchain_image_index];
    }

    void RenderGraphResourceRegistry::reset() {
        m_texture_handles.clear();
        m_buffer_handles.clear();
        m_backbuffer = nullptr;
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
