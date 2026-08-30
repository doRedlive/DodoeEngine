// do@Redlive

#include "binding_set_cache.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

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

    GfxBindingSetHandle BindingSetCache::getOrCreate(const GfxBindingSetDesc& desc,
                                                       GfxBindingLayoutHandle layout,
                                                       UInt64 layout_generation) {
        Size_t h = reinterpret_cast<Size_t>(layout.Get());
        hash_combine(h, static_cast<Size_t>(layout_generation));
        hash_combine(h, desc.bindings.size());
        for (Size_t i = 0; i < desc.bindings.size(); ++i) {
            const auto& item = desc.bindings[i];
            hash_combine(h, reinterpret_cast<Size_t>(item.resourceHandle));
            hash_combine(h, static_cast<Size_t>(item.slot));
            hash_combine(h, static_cast<Size_t>(item.arrayElement));
            hash_combine(h, static_cast<Size_t>(item.type));
            hash_combine(h, static_cast<Size_t>(item.format));
            hash_combine(h, static_cast<Size_t>(item.dimension));
            hash_combine(h, static_cast<Size_t>(item.rawData[0]));
            hash_combine(h, static_cast<Size_t>(item.rawData[1]));
        }

        auto it = m_cache.find(h);
        if (it != m_cache.end() && it->second->isGpuReady()) {
            return it->second;
        }

        auto binding_set = GDrawCommandList.createBindingSet(desc, layout);
        m_cache[h] = binding_set;
        // DO_DEBUG("BindingSetCache: created binding set (cache size={})", m_cache.size());
        return binding_set;
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
