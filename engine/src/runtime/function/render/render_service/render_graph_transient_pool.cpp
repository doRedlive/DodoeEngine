// do@Redlive

#include "render_graph_transient_pool.h"

#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    GfxTextureHandle RenderGraphTransientPool::acquireTexture(const GfxTextureDesc& desc,
                                                                DrawCommandList& command_list) {
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
        return texture;
    }

    GfxBufferHandle RenderGraphTransientPool::acquireBuffer(const GfxBufferDesc& desc,
                                                              DrawCommandList& command_list) {
        for (Size_t i = 0; i < m_buffers.size(); i++) {
            if (!m_buffer_in_use[i]) {
                const auto& pooled = m_buffers[i].desc;
                if (pooled.byteSize == desc.byteSize && pooled.structStride == desc.structStride &&
                    pooled.format == desc.format && pooled.canHaveUAVs == desc.canHaveUAVs &&
                    pooled.canHaveTypedViews == desc.canHaveTypedViews &&
                    pooled.isVertexBuffer == desc.isVertexBuffer) {
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

    void RenderGraphTransientPool::releaseAll() {
        for (auto& in_use : m_texture_in_use) {
            in_use = false;
        }
        for (auto& in_use : m_buffer_in_use) {
            in_use = false;
        }
    }

    void RenderGraphTransientPool::reset() {
        m_textures.clear();
        m_buffers.clear();
        m_texture_in_use.clear();
        m_buffer_in_use.clear();
    }

} // namespace dodoe
