// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include <mutex>

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
        GfxBindingSetHandle getOrCreate(DrawCommandList& command_list,
                                         const GfxLogicalBindingSetDesc& desc,
                                         GfxBindingLayoutHandle layout,
                                         UInt64 layout_generation);
        void invalidateFor(const void* resource_ptr);
        void invalidateForLayout(GfxBindingLayoutHandle layout);
        void clear();
        [[nodiscard]] Size_t size() const { std::lock_guard<std::mutex> lock(m_mutex); return m_cache.size(); }

    private:
        Bool initialize(const BindingSetCacheCreateInfo& info);
        void shutdown();

        GfxContext* m_gfx_context{nullptr};
        mutable std::mutex m_mutex{};
        struct CacheKey {
            GfxBindingLayoutHandle layout{};
            UInt64 generation{0};
            GfxLogicalBindingSetDesc desc{};
            Size_t hash{0};
            Bool operator==(const CacheKey& other) const;
        };
        struct CacheKeyHash {
            Size_t operator()(const CacheKey& key) const { return key.hash; }
        };
        UnorderedMap<CacheKey, GfxBindingSetHandle, CacheKeyHash> m_cache{};
    };

} // namespace dodoe
