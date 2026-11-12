// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include <mutex>

namespace dodoe {

    class GfxContext;
    class DrawCommandList;

    struct BindingLayoutCacheCreateInfo {
        GfxContext* gfx_context{nullptr};
    };

    class BindingLayoutCache : public Managed<BindingLayoutCache, BindingLayoutCacheCreateInfo> {
        friend class Managed<BindingLayoutCache, BindingLayoutCacheCreateInfo>;

    public:
        GfxBindingLayoutHandle getOrCreate(const GfxBindingLayoutDesc& desc);
        UInt64 getLayoutGeneration(GfxBindingLayoutHandle layout) const;
        void clear();

    private:
        Bool initialize(const BindingLayoutCacheCreateInfo& info);
        void shutdown();

        struct LayoutEntry {
            GfxBindingLayoutHandle layout{};
            UInt64 generation{0};
            GfxBindingLayoutDesc desc{};
            Size_t hash{0};
        };

        GfxContext* m_gfx_context{nullptr};
        mutable std::mutex m_mutex{};
        DynamicArray<LayoutEntry> m_cache{};
        UInt64 m_next_generation{1};
    };

} // namespace dodoe
