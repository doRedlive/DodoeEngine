// do@Redlive

#pragma once

#include "../interface/rhi.h"

namespace dodoe {

    class RhiContext;

    using DescriptorIndex = int;

    struct DescriptorTableManagerCreateInfo {
        RhiContext* rhi;
    };

    class DescriptorTableManager : public Managed<DescriptorTableManager, DescriptorTableManagerCreateInfo> {
        friend class Managed<DescriptorTableManager, DescriptorTableManagerCreateInfo>;
        struct BindingSetItemHasher {
            std::size_t operator()(const rhi::BindingSetItem& item) const {
                size_t hash = 0;
                rhi::hash_combine(hash, item.resourceHandle);
                rhi::hash_combine(hash, item.type);
                rhi::hash_combine(hash, item.format);
                rhi::hash_combine(hash, item.dimension);
                rhi::hash_combine(hash, item.rawData[0]);
                rhi::hash_combine(hash, item.rawData[1]);
                return hash;
            }
        };

        struct BindingSetItemsEqual {
            bool operator()(const rhi::BindingSetItem& a, const rhi::BindingSetItem& b) const {
                return a.resourceHandle == b.resourceHandle
                    && a.type == b.type
                    && a.format == b.format
                    && a.dimension == b.dimension
                    && a.subresources == b.subresources;
            }
        };
        
        rhi::DescriptorTableHandle descriptor_table_{};
        std::vector<rhi::BindingSetItem> descriptors_{};
        std::unordered_map<rhi::BindingSetItem, DescriptorIndex, BindingSetItemHasher, BindingSetItemsEqual> descriptor_index_umap_{};
        std::vector<bool> allocated_descriptors_{};
        int search_start_ = 0;

        RhiContext* rhi_{nullptr};

    public:

        DescriptorIndex createDescriptor(rhi::BindingSetItem item);
        [[nodiscard]] rhi::IDescriptorTable* getDescriptorTable() const { return descriptor_table_; }

    private:
        bool initialize(const DescriptorTableManagerCreateInfo& info);
        void shutdown();
    };

} // dodoe
