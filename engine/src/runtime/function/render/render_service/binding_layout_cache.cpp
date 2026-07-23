// do@Redlive

#include "binding_layout_cache.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    namespace {
        Size_t HashBindingLayoutDesc(const GfxBindingLayoutDesc& desc) {
            Size_t h = 0;
            hash_combine(h, static_cast<Size_t>(desc.getVisibility()));
            for (Size_t i = 0; i < desc.getItemCount(); ++i) {
                hash_combine(h, static_cast<Size_t>(desc.getItems()[i].type));
                hash_combine(h, static_cast<Size_t>(desc.getItems()[i].slot));
            }
            return h;
        }
    } // anonymous namespace

    Bool BindingLayoutCache::initialize(const BindingLayoutCacheCreateInfo& info) {
        m_gfx_context = info.gfx_context;
        DO_ASSERT(m_gfx_context != nullptr, "BindingLayoutCache requires gfx_context");
        return true;
    }

    void BindingLayoutCache::shutdown() {
        m_cache.clear();
        m_gfx_context = nullptr;
    }

    GfxBindingLayoutHandle BindingLayoutCache::getOrCreate(const GfxBindingLayoutDesc& desc,
                                                             DrawCommandList& cmd) {
        const auto hash = HashBindingLayoutDesc(desc);
        auto it = m_cache.find(hash);
        if (it != m_cache.end()) {
            return it->second.layout;
        }

        auto layout = cmd.createBindingLayout(desc);
        m_cache[hash] = {layout, m_next_generation++};
        return layout;
    }

    UInt64 BindingLayoutCache::getLayoutGeneration(GfxBindingLayoutHandle layout) const {
        for (const auto& [hash, entry] : m_cache) {
            if (entry.layout == layout) {
                return entry.generation;
            }
        }
        return 0;
    }

    void BindingLayoutCache::clear() {
        m_cache.clear();
        m_next_generation = 1;
    }

} // namespace dodoe
