// do@Redlive

#include "render_graph_transient_pool.h"

#include "runtime/function/graphics/draw_command_list.h"

#include <atomic>

namespace dodoe {

#ifdef DODOE_PERF_ENABLED
    static std::atomic<Size_t> s_texture_count{0};
    static std::atomic<UInt64> s_texture_bytes{0};
    static std::atomic<Size_t> s_buffer_count{0};
    static std::atomic<UInt64> s_buffer_bytes{0};

    static UInt64 EstimateTextureBytes(const GfxTextureDesc& desc) {
        return static_cast<UInt64>(desc.width) * desc.height * desc.depth * desc.arraySize * 4;
    }
#endif

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
#ifdef DODOE_PERF_ENABLED
        s_texture_count.fetch_add(1, std::memory_order_relaxed);
        s_texture_bytes.fetch_add(EstimateTextureBytes(desc), std::memory_order_relaxed);
#endif
        return texture;
    }

    GfxBufferHandle RenderGraphTransientPool::acquireBuffer(const GfxBufferDesc& desc,
                                                              DrawCommandList& command_list) {
        DO_PROFILE_SCOPE_CATEGORY("RenderGraphTransientPool::acquireBuffer", "resource-cache");
        constexpr Size_t kInvalidIndex = static_cast<Size_t>(-1);
        Size_t best = kInvalidIndex;
        for (Size_t i = 0; i < m_buffers.size(); i++) {
            if (m_buffer_in_use[i]) {
                continue;
            }
            const auto& pooled = m_buffers[i].desc;
            if (pooled.byteSize < desc.byteSize ||
                pooled.structStride != desc.structStride ||
                pooled.format != desc.format ||
                pooled.canHaveUAVs != desc.canHaveUAVs ||
                pooled.canHaveTypedViews != desc.canHaveTypedViews ||
                pooled.isVertexBuffer != desc.isVertexBuffer ||
                pooled.isIndexBuffer != desc.isIndexBuffer ||
                pooled.isConstantBuffer != desc.isConstantBuffer ||
                pooled.isDrawIndirectArgs != desc.isDrawIndirectArgs ||
                pooled.isAccelStructBuildInput != desc.isAccelStructBuildInput ||
                pooled.isAccelStructStorage != desc.isAccelStructStorage ||
                pooled.isShaderBindingTable != desc.isShaderBindingTable ||
                pooled.isVolatile != desc.isVolatile) {
                continue;
            }
            if (best == kInvalidIndex || pooled.byteSize < m_buffers[best].desc.byteSize) {
                best = i;
            }
        }
        if (best != kInvalidIndex) {
            m_buffer_in_use[best] = true;
            return m_buffers[best].buffer;
        }
        const auto buffer = command_list.createBuffer(desc);
        m_buffers.push_back({buffer, desc});
        m_buffer_in_use.push_back(true);
#ifdef DODOE_PERF_ENABLED
        s_buffer_count.fetch_add(1, std::memory_order_relaxed);
        s_buffer_bytes.fetch_add(desc.byteSize, std::memory_order_relaxed);
#endif
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
#ifdef DODOE_PERF_ENABLED
        for (const auto& pooled : m_textures) {
            s_texture_count.fetch_sub(1, std::memory_order_relaxed);
            s_texture_bytes.fetch_sub(EstimateTextureBytes(pooled.desc), std::memory_order_relaxed);
        }
        for (const auto& pooled : m_buffers) {
            s_buffer_count.fetch_sub(1, std::memory_order_relaxed);
            s_buffer_bytes.fetch_sub(pooled.desc.byteSize, std::memory_order_relaxed);
        }
#endif
        m_textures.clear();
        m_buffers.clear();
        m_texture_in_use.clear();
        m_buffer_in_use.clear();
    }

#ifdef DODOE_PERF_ENABLED
    RenderGraphTransientPool::GlobalStats RenderGraphTransientPool::QueryGlobalStats() {
        return {s_texture_count.load(std::memory_order_relaxed),
                s_texture_bytes.load(std::memory_order_relaxed),
                s_buffer_count.load(std::memory_order_relaxed),
                s_buffer_bytes.load(std::memory_order_relaxed)};
    }
#endif

} // namespace dodoe
