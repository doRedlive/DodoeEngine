// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class BindingSetAllocator {
    public:
        struct BindingSetCacheKey {
            GfxBindingLayoutHandle layout;
            UnorderedMap<UInt32, GfxBindingSetItem> items;

            Bool operator==(const BindingSetCacheKey& other) const {
                return layout == other.layout && items == other.items;
            }
        };

        struct BindingSetCacheKeyHash {
            Size_t operator()(const BindingSetCacheKey& key) const {
                Size_t hash = reinterpret_cast<Size_t>(key.layout.Get());
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
