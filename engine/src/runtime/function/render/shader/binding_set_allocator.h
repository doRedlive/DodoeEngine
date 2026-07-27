// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class DrawCommandList;
    class BindingSetAllocator {
    public:
        struct BindingSetCacheKey {
            GfxBindingLayoutHandle layout;
            UInt64 layout_generation{0};
            UnorderedMap<UInt32, GfxBindingSetItem> items;

            Bool operator==(const BindingSetCacheKey& other) const {
                return layout_generation == other.layout_generation && items == other.items;
            }
        };

        struct BindingSetCacheKeyHash {
            Size_t operator()(const BindingSetCacheKey& key) const {
                Size_t hash = key.layout_generation;
                for (const auto& [slot, item] : key.items) {
                    hash_combine(hash, slot);
                    hash_combine(hash, item.resourceHandle);
                    hash_combine(hash, item.type);
                }
                return hash;
            }
        };

        GfxBindingSetHandle getOrCreate(const BindingSetCacheKey& key,
                                        GfxDeviceHandle device);

        GfxBindingSetHandle getOrCreate(const BindingSetCacheKey& key,
                                        DrawCommandList& cmd_list);

        void clear();

    private:
        UnorderedMap<BindingSetCacheKey, GfxBindingSetHandle, BindingSetCacheKeyHash> m_cache;
    };

} // namespace dodoe
