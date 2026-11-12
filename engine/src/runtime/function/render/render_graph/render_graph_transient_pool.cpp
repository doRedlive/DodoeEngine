// do@Redlive

#include "render_graph_transient_pool.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/render_service/framebuffer_cache.h"
#include "runtime/function/render/render_service/binding_set_cache.h"

#include <atomic>

namespace dodoe {

#ifdef DODOE_PERF_ENABLED
    static std::atomic<Size_t> s_texture_count{0};
    static std::atomic<UInt64> s_texture_bytes{0};
    static std::atomic<Size_t> s_buffer_count{0};
    static std::atomic<UInt64> s_buffer_bytes{0};
#endif

    static UInt64 EstimateTextureBytes(const GfxTextureDesc& desc) {
        UInt64 bytes = static_cast<UInt64>(desc.width) * desc.height * desc.depth * desc.arraySize;
        bytes *= desc.sampleCount * desc.sampleCount;
        bytes *= 4;
        return bytes;
    }

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
                    m_textures[i].stamp = m_stamp;
                    return m_textures[i].texture;
                }
            }
        }
        const auto texture = command_list.createTexture(desc);
        if (!texture) {
            return nullptr;
        }
        PooledTexture entry{};
        entry.texture = texture;
        entry.desc = desc;
        entry.bytes = EstimateTextureBytes(desc);
        entry.stamp = m_stamp;
#ifdef DODOE_PERF_ENABLED
        s_texture_count.fetch_add(1, std::memory_order_relaxed);
        s_texture_bytes.fetch_add(entry.bytes, std::memory_order_relaxed);
#endif
        m_texture_bytes += entry.bytes;
        m_textures.push_back(std::move(entry));
        m_texture_in_use.push_back(true);
        enforceTextureBudget();
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
            m_buffers[best].stamp = m_stamp;
            return m_buffers[best].buffer;
        }
        const auto buffer = command_list.createBuffer(desc);
        if (!buffer) {
            return nullptr;
        }
        PooledBuffer entry{};
        entry.buffer = buffer;
        entry.desc = desc;
        entry.bytes = desc.byteSize;
        entry.stamp = m_stamp;
        m_buffer_bytes += entry.bytes;
        m_buffers.push_back(std::move(entry));
        m_buffer_in_use.push_back(true);
#ifdef DODOE_PERF_ENABLED
        s_buffer_count.fetch_add(1, std::memory_order_relaxed);
        s_buffer_bytes.fetch_add(desc.byteSize, std::memory_order_relaxed);
#endif
        enforceBufferBudget();
        return buffer;
    }

    void RenderGraphTransientPool::enforceTextureBudget() {
        while (m_texture_bytes > m_texture_budget_bytes) {
            constexpr Size_t kInvalidIndex = static_cast<Size_t>(-1);
            Size_t victim = kInvalidIndex;
            for (Size_t i = 0; i < m_textures.size(); i++) {
                if (m_texture_in_use[i]) {
                    continue;
                }
                if (victim == kInvalidIndex || m_textures[i].stamp < m_textures[victim].stamp) {
                    victim = i;
                }
            }
            if (victim == kInvalidIndex) {
                break;
            }
#ifdef DODOE_PERF_ENABLED
            s_texture_count.fetch_sub(1, std::memory_order_relaxed);
            s_texture_bytes.fetch_sub(m_textures[victim].bytes, std::memory_order_relaxed);
#endif
            m_texture_bytes -= m_textures[victim].bytes;
            m_pending_eviction_textures.push_back(std::move(m_textures[victim].texture));
            m_textures[victim] = std::move(m_textures.back());
            m_texture_in_use[victim] = m_texture_in_use.back();
            m_textures.pop_back();
            m_texture_in_use.pop_back();
        }
    }

    void RenderGraphTransientPool::enforceBufferBudget() {
        while (m_buffer_bytes > m_buffer_budget_bytes) {
            constexpr Size_t kInvalidIndex = static_cast<Size_t>(-1);
            Size_t victim = kInvalidIndex;
            for (Size_t i = 0; i < m_buffers.size(); i++) {
                if (m_buffer_in_use[i]) {
                    continue;
                }
                if (victim == kInvalidIndex || m_buffers[i].stamp < m_buffers[victim].stamp) {
                    victim = i;
                }
            }
            if (victim == kInvalidIndex) {
                break;
            }
#ifdef DODOE_PERF_ENABLED
            s_buffer_count.fetch_sub(1, std::memory_order_relaxed);
            s_buffer_bytes.fetch_sub(m_buffers[victim].bytes, std::memory_order_relaxed);
#endif
            m_buffer_bytes -= m_buffers[victim].bytes;
            m_pending_eviction_buffers.push_back(std::move(m_buffers[victim].buffer));
            m_buffers[victim] = std::move(m_buffers.back());
            m_buffer_in_use[victim] = m_buffer_in_use.back();
            m_buffers.pop_back();
            m_buffer_in_use.pop_back();
        }
    }

    void RenderGraphTransientPool::flushEvictions(FramebufferCache* framebuffer_cache,
                                                   BindingSetCache* binding_set_cache) {
        for (const auto& texture : m_pending_eviction_textures) {
            if (framebuffer_cache) {
                framebuffer_cache->invalidateTexture(texture.Get());
            }
            if (binding_set_cache) {
                binding_set_cache->invalidateFor(texture.Get());
            }
        }
        m_pending_eviction_textures.clear();

        for (const auto& buffer : m_pending_eviction_buffers) {
            if (binding_set_cache) {
                binding_set_cache->invalidateFor(buffer.Get());
            }
        }
        m_pending_eviction_buffers.clear();
    }

    void RenderGraphTransientPool::releaseAll() {
        DO_PROFILE_SCOPE_CATEGORY("RenderGraphTransientPool::releaseAll", "resource-cache");
        for (auto& in_use : m_texture_in_use) {
            in_use = false;
        }
        for (auto& in_use : m_buffer_in_use) {
            in_use = false;
        }
        ++m_stamp;
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
        m_pending_eviction_textures.clear();
        m_pending_eviction_buffers.clear();
        m_texture_bytes = 0;
        m_buffer_bytes = 0;
        m_stamp = 0;
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
