// do@Redlive

#include "binding_set_cache.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    Bool BindingSetCache::initialize(const BindingSetCacheCreateInfo& info) {
        m_gfx_context = info.gfx_context;
        DO_ASSERT(m_gfx_context != nullptr, "BindingSetCache requires gfx_context");
        return true;
    }

    void BindingSetCache::shutdown() {
        m_cache.clear();
        m_gfx_context = nullptr;
    }

    GfxBindingSetHandle BindingSetCache::getOrCreate(const GfxBindingSetDesc& desc,
                                                       GfxBindingLayoutHandle layout,
                                                       UInt64 layout_generation) {
        Size_t h = reinterpret_cast<Size_t>(layout.Get());
        hash_combine(h, static_cast<Size_t>(layout_generation));
        hash_combine(h, desc.getItemCount());
        for (Size_t i = 0; i < desc.getItemCount(); ++i) {
            const auto& item = desc.getItems()[i];
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
        if (it != m_cache.end()) {
            return it->second;
        }

        auto binding_set = GDrawCommandList.createBindingSet(desc, layout);
        m_cache[h] = binding_set;
        return binding_set;
    }

    void BindingSetCache::invalidateForLayout(GfxBindingLayoutHandle layout) {
        // 简单策略：layout 变化时清除全部缓存
        // 更精细的实现可以只清除引用该 layout 的条目
        m_cache.clear();
    }

    void BindingSetCache::clear() {
        m_cache.clear();
    }

} // namespace dodoe
