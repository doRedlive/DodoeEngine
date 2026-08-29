// do@Redlive

#include "render_graph_transient_pool.h"

#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    GfxTextureHandle RenderGraphTransientPool::acquireTexture(const GfxTextureDesc& desc,
                                                                DrawCommandList& command_list) {
        DO_PROFILE_SCOPE_CATEGORY("RenderGraphTransientPool::acquireTexture", "resource-cache");
        for (Size_t i = 0; i < m_textures.size(); i++) {
            if (!m_texture_in_use[i]) {
                const auto& pooled = m_textures[i].desc;
                if (pooled.width == desc.width && pooled.height == desc.height &&
                    pooled.depth == desc.depth && pooled.arraySize == desc.arraySize &&
                    pooled.mipLevels == desc.mipLevels && pooled.sampleCount == desc.sampleCount &&
                    pooled.format == desc.format && pooled.dimension == desc.dimension) {
                    m_texture_in_use[i] = true;
                    return m_textures[i].texture;
                }
            }
        }
        const auto texture = command_list.createTexture(desc);
        m_textures.push_back({texture, desc});
        m_texture_in_use.push_back(true);
        DO_DEBUG("RenderGraphTransientPool: created texture slot {} ({}x{})",
            m_textures.size() - 1, desc.width, desc.height);
        return texture;
    }

    GfxBufferHandle RenderGraphTransientPool::acquireBuffer(const GfxBufferDesc& desc,
                                                              DrawCommandList& command_list) {
        DO_PROFILE_SCOPE_CATEGORY("RenderGraphTransientPool::acquireBuffer", "resource-cache");
        for (Size_t i = 0; i < m_buffers.size(); i++) {
            if (!m_buffer_in_use[i]) {
                const auto& pooled = m_buffers[i].desc;
                if (pooled.byteSize == desc.byteSize && pooled.structStride == desc.structStride &&
                    pooled.format == desc.format && pooled.canHaveUAVs == desc.canHaveUAVs &&
                    pooled.canHaveTypedViews == desc.canHaveTypedViews &&
                    pooled.isVertexBuffer == desc.isVertexBuffer &&
                    pooled.isIndexBuffer == desc.isIndexBuffer &&
                    pooled.isConstantBuffer == desc.isConstantBuffer &&
                    pooled.isDrawIndirectArgs == desc.isDrawIndirectArgs &&
                    pooled.isAccelStructBuildInput == desc.isAccelStructBuildInput &&
                    pooled.isAccelStructStorage == desc.isAccelStructStorage &&
                    pooled.isShaderBindingTable == desc.isShaderBindingTable &&
                    pooled.isVolatile == desc.isVolatile) {
                    m_buffer_in_use[i] = true;
                    return m_buffers[i].buffer;
                }
            }
        }
        const auto buffer = command_list.createBuffer(desc);
        m_buffers.push_back({buffer, desc});
        m_buffer_in_use.push_back(true);
        DO_DEBUG("RenderGraphTransientPool: created buffer slot {} (size={})",
            m_buffers.size() - 1, desc.byteSize);
        return buffer;
    }

    void RenderGraphTransientPool::releaseAll() {
        DO_PROFILE_SCOPE_CATEGORY("RenderGraphTransientPool::releaseAll", "resource-cache");
        for (auto& in_use : m_texture_in_use) {
            in_use = false;
        }
        for (auto& in_use : m_buffer_in_use) {
            in_use = false;
        }
    }

    void RenderGraphTransientPool::reset() {
        DO_PROFILE_SCOPE_CATEGORY("RenderGraphTransientPool::reset", "shutdown");
        DO_INFO("RenderGraphTransientPool: releasing {} texture(s) and {} buffer(s)",
            m_textures.size(), m_buffers.size());
        m_textures.clear();
        m_buffers.clear();
        m_texture_in_use.clear();
        m_buffer_in_use.clear();
    }

} // namespace dodoe
