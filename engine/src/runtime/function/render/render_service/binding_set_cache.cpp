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
        Size_t h = layout_generation;
        for (Size_t i = 0; i < desc.getItemCount(); ++i) {
            hash_combine(h, static_cast<Size_t>(desc.getItems()[i].type));
            hash_combine(h, static_cast<Size_t>(desc.getItems()[i].slot));
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
