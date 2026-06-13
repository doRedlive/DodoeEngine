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
            std::size_t operator()(const gfx::BindingSetItem& item) const {
                size_t hash = 0;
                gfx::hash_combine(hash, item.resourceHandle);
                gfx::hash_combine(hash, item.type);
                gfx::hash_combine(hash, item.format);
                gfx::hash_combine(hash, item.dimension);
                gfx::hash_combine(hash, item.rawData[0]);
                gfx::hash_combine(hash, item.rawData[1]);
                return hash;
            }
        };

        struct BindingSetItemsEqual {
            bool operator()(const gfx::BindingSetItem& a, const gfx::BindingSetItem& b) const {
                return a.resourceHandle == b.resourceHandle
                    && a.type == b.type
                    && a.format == b.format
                    && a.dimension == b.dimension
                    && a.subresources == b.subresources;
            }
        };
        
        gfx::DescriptorTableHandle descriptor_table_{};
        std::vector<gfx::BindingSetItem> descriptors_{};
        std::unordered_map<gfx::BindingSetItem, DescriptorIndex, BindingSetItemHasher, BindingSetItemsEqual> descriptor_index_umap_{};
        std::vector<bool> allocated_descriptors_{};
        int search_start_ = 0;

        GfxContext* gfx_{nullptr};

    public:

        DescriptorIndex createDescriptor(gfx::BindingSetItem item);
        void releaseDescriptor(DescriptorIndex index);
        [[nodiscard]] gfx::IDescriptorTable* getDescriptorTable() const { return descriptor_table_; }

    private:
        bool initialize(const DescriptorTableManagerCreateInfo& info);
        void shutdown();
    };

} // dodoe
