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

    void FramebufferCache::initialize(GfxContext& gfx) {
        m_gfx_context = &gfx;
    }

    GfxFramebufferHandle FramebufferCache::getOrCreate(const FramebufferCacheKey& key,
                                                         const GfxFramebufferDesc& desc) {
        const auto hash = key.computeHash();
        for (const auto& entry : m_entries) {
            if (entry.key == key) {
                return entry.framebuffer;
            }
        }

        auto framebuffer = GDrawCommandList.createFramebuffer(desc);
        m_entries.push_back({key, framebuffer});
        return framebuffer;
    }

    void FramebufferCache::invalidateTexture(const void* texture_ptr) {
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
    }

    void FramebufferCache::endFrame() {

    }

    void FramebufferCache::reset() {
        m_entries.clear();
    }

} // namespace dodoe
