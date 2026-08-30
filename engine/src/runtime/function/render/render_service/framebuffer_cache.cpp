// do@Redlive

#include "framebuffer_cache.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    Size_t FramebufferCacheKey::computeHash() const {
        Size_t h = 0;
        for (const auto& ref : color_refs) {
            hash_combine(h, ref.texture_ptr);
            hash_combine(h, ref.revision);
            hash_combine(h, ref.mip);
            hash_combine(h, ref.layer);
        }
        hash_combine(h, depth_ref.texture_ptr);
        hash_combine(h, depth_ref.revision);
        hash_combine(h, depth_ref.mip);
        hash_combine(h, depth_ref.layer);
        hash_combine(h, sample_count);
        return h;
    }

    Bool FramebufferCache::initialize(const FramebufferCacheCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("FramebufferCache::initialize", "startup");
        m_gfx_context = info.gfx_context;
        if (!m_gfx_context) {
            DO_ERROR("FramebufferCache::initialize: graphics context is unavailable");
        }
        return m_gfx_context != nullptr;
    }

    GfxFramebufferHandle FramebufferCache::getOrCreate(const FramebufferCacheKey& key,
                                                         const GfxFramebufferDesc& desc) {
        DO_PROFILE_SCOPE_CATEGORY("FramebufferCache::getOrCreate", "resource-cache");
        const auto hash = key.computeHash();
        for (auto& entry : m_entries) {
            if (entry.key == key) {
                if (entry.framebuffer->isGpuReady()) {
                    return entry.framebuffer;
                }
                auto framebuffer = GDrawCommandList.createFramebuffer(desc);
                entry.framebuffer = framebuffer;
                return framebuffer;
            }
        }

        auto framebuffer = GDrawCommandList.createFramebuffer(desc);
        m_entries.push_back({key, framebuffer});
        // DO_DEBUG("FramebufferCache: created framebuffer (cache size={})", m_entries.size());
        return framebuffer;
    }

    void FramebufferCache::invalidateTexture(const void* texture_ptr) {
        DO_PROFILE_SCOPE_CATEGORY("FramebufferCache::invalidateTexture", "resource-cache");
        const Size_t old_size = m_entries.size();
        Size_t write_index = 0;
        for (Size_t i = 0; i < m_entries.size(); ++i) {
            Bool references_texture = false;
            for (const auto& ref : m_entries[i].key.color_refs) {
                if (ref.texture_ptr == texture_ptr) {
                    references_texture = true;
                    break;
                }
            }
            if (!references_texture &&
                m_entries[i].key.depth_ref.texture_ptr != texture_ptr) {
                if (write_index != i) {
                    m_entries[write_index] = std::move(m_entries[i]);
                }
                ++write_index;
            }
        }
        m_entries.resize(write_index);
        // DO_DEBUG("FramebufferCache: invalidated {} framebuffer(s)", old_size - m_entries.size());
    }

    void FramebufferCache::endFrame() {

    }

    void FramebufferCache::reset() {
        DO_PROFILE_SCOPE_CATEGORY("FramebufferCache::reset", "shutdown");
        m_entries.clear();
    }

    void FramebufferCache::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("FramebufferCache::shutdown", "shutdown");
        DO_INFO("FramebufferCache: releasing {} cached framebuffer(s)", m_entries.size());
        reset();
        m_gfx_context = nullptr;
    }

} // namespace dodoe
