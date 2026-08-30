// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class GfxContext;
    class DrawCommandList;

    enum class BindingSetFrequency : UInt8 {
        Global,
        Material,
        Pass,
        Draw,
    };

    struct BindingSetCacheCreateInfo {
        GfxContext* gfx_context{nullptr};
    };

    class BindingSetCache : public Managed<BindingSetCache, BindingSetCacheCreateInfo> {
        friend class Managed<BindingSetCache, BindingSetCacheCreateInfo>;

    public:
        GfxBindingSetHandle getOrCreate(const GfxBindingSetDesc& desc,
                                         GfxBindingLayoutHandle layout,
                                         UInt64 layout_generation);
        void invalidateForLayout(GfxBindingLayoutHandle layout);
        void clear();
        [[nodiscard]] Size_t size() const { return m_cache.size(); }

    private:
        Bool initialize(const BindingSetCacheCreateInfo& info);
        void shutdown();

        GfxContext* m_gfx_context{nullptr};
        UnorderedMap<Size_t, GfxBindingSetHandle> m_cache{};
    };

} // namespace dodoe
