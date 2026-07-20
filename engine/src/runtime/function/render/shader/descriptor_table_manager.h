// do@Redlive

#pragma once

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class GfxContext;

    using DescriptorIndex = int;

    struct DescriptorTableManagerCreateInfo {
        GfxContext* gfx;
    };

    class DescriptorTableManager : public Managed<DescriptorTableManager, DescriptorTableManagerCreateInfo> {
        friend class Managed<DescriptorTableManager, DescriptorTableManagerCreateInfo>;
        struct BindingSetItemHasher {
            std::size_t operator()(const GfxBindingSetItem& item) const {
                size_t hash = 0;
                hash_combine(hash, item.resourceHandle);
                hash_combine(hash, item.type);
                hash_combine(hash, item.format);
                hash_combine(hash, item.dimension);
                hash_combine(hash, item.rawData[0]);
                hash_combine(hash, item.rawData[1]);
                return hash;
            }
        };

        struct BindingSetItemsEqual {
            bool operator()(const GfxBindingSetItem& a, const GfxBindingSetItem& b) const {
                return a.resourceHandle == b.resourceHandle
                    && a.type == b.type
                    && a.format == b.format
                    && a.dimension == b.dimension
                    && a.subresources == b.subresources;
            }
        };
        
        GfxDescriptorTableHandle descriptor_table_{};
        std::vector<GfxBindingSetItem> descriptors_{};
        std::unordered_map<GfxBindingSetItem, DescriptorIndex, BindingSetItemHasher, BindingSetItemsEqual> descriptor_index_umap_{};
        std::vector<bool> allocated_descriptors_{};
        int search_start_ = 0;

        GfxContext* gfx_{nullptr};

    public:

        DescriptorIndex createDescriptor(GfxBindingSetItem item);
        void releaseDescriptor(DescriptorIndex index);
        UInt32 allocateSlot();
        [[nodiscard]] GfxDescriptorTable* getDescriptorTable() const { return descriptor_table_; }

    private:
        bool initialize(const DescriptorTableManagerCreateInfo& info);
        void shutdown();
    };

} // dodoe
