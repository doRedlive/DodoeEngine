// do@Redlive

#include "binding_set_cache.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"


namespace dodoe {

    namespace {

        Size_t HashLogicalItem(Size_t h, const GfxLogicalBindingItem& item) {
            hash_combine(h, static_cast<Size_t>(item.type));
            hash_combine(h, static_cast<Size_t>(item.slot));
            hash_combine(h, static_cast<Size_t>(item.array_element));
            hash_combine(h, reinterpret_cast<Size_t>(item.texture.Get()));
            hash_combine(h, reinterpret_cast<Size_t>(item.buffer.Get()));
            hash_combine(h, reinterpret_cast<Size_t>(item.sampler.Get()));
            hash_combine(h, static_cast<Size_t>(item.format));
            hash_combine(h, static_cast<Size_t>(item.dimension));
            hash_combine(h, static_cast<Size_t>(item.raw_data[0]));
            hash_combine(h, static_cast<Size_t>(item.raw_data[1]));
            return h;
        }

    } // namespace

    Bool BindingSetCache::initialize(const BindingSetCacheCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("BindingSetCache::initialize", "startup");
        m_gfx_context = info.gfx_context;
        DO_ASSERT(m_gfx_context != nullptr, "BindingSetCache requires gfx_context");
        if (!m_gfx_context) {
            DO_ERROR("BindingSetCache::initialize: graphics context is unavailable");
        }
        return m_gfx_context != nullptr;
    }

    void BindingSetCache::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("BindingSetCache::shutdown", "shutdown");
        DO_INFO("BindingSetCache: releasing {} cached binding set(s)", m_cache.size());
        m_cache.clear();
        m_gfx_context = nullptr;
    }

    GfxBindingSetHandle BindingSetCache::getOrCreate(DrawCommandList& command_list,
                                                       const GfxLogicalBindingSetDesc& desc,
                                                       GfxBindingLayoutHandle layout,
                                                       UInt64 layout_generation) {
        Size_t h = reinterpret_cast<Size_t>(layout.Get());
        hash_combine(h, static_cast<Size_t>(layout_generation));
        hash_combine(h, desc.bindings.size());
        for (const auto& item : desc.bindings) {
            h = HashLogicalItem(h, item);
        }

        auto it = m_cache.find(h);
        if (it != m_cache.end()) {
            if (!it->second) {
                m_cache.erase(it);
            } else if (it->second->isGpuReady()) {
                return it->second;
            } else {
                command_list.requestRealize(it->second);
                return it->second;
            }
        }

        auto binding_set = command_list.createBindingSet(desc, layout);
        m_cache[h] = binding_set;
        // DO_DEBUG("BindingSetCache: created binding set (cache size={})", m_cache.size());
        return binding_set;
    }

    void BindingSetCache::invalidateFor(const void* resource_ptr) {
        DO_PROFILE_SCOPE_CATEGORY("BindingSetCache::invalidateFor", "resource-cache");
        for (auto it = m_cache.begin(); it != m_cache.end();) {
            Bool references = false;
            if (it->second) {
                const auto& logical_desc = it->second->getLogicalDesc();
                for (const auto& item : logical_desc.bindings) {
                    if (item.texture.Get() == resource_ptr || item.buffer.Get() == resource_ptr) {
                        references = true;
                        break;
                    }
                }
            }
            if (references) {
                it = m_cache.erase(it);
            } else {
                ++it;
            }
        }
    }

    void BindingSetCache::invalidateForLayout(GfxBindingLayoutHandle layout) {
        DO_PROFILE_SCOPE_CATEGORY("BindingSetCache::invalidateForLayout", "resource-cache");
        // DO_DEBUG("BindingSetCache: invalidating {} entries for layout change", m_cache.size());
        m_cache.clear();
    }

    void BindingSetCache::clear() {
        DO_PROFILE_SCOPE_CATEGORY("BindingSetCache::clear", "resource-cache");
        m_cache.clear();
    }

} // namespace dodoe
