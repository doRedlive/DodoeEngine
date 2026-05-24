// do@Redlive

#include "descriptor_table_manager.h"

#include "../interface/rhi_context.h"

namespace dodoe {

    bool DescriptorTableManager::initialize(const DescriptorTableManagerCreateInfo& info) {
        rhi_ = info.rhi;

        rhi::BindlessLayoutDesc bindless_layout_desc;
        bindless_layout_desc.visibility = rhi::ShaderType::All;
        bindless_layout_desc.firstSlot = 0;
        bindless_layout_desc.maxCapacity = 1024;
        bindless_layout_desc.registerSpaces = {
            rhi::BindingLayoutItem::Texture_SRV(0)
        };
        auto bindless_layout = rhi_->getDevice()->createBindlessLayout(bindless_layout_desc);
        descriptor_table_ = rhi_->getDevice()->createDescriptorTable(bindless_layout);

        size_t capacity = descriptor_table_->getCapacity();
        allocated_descriptors_.resize(capacity);
        descriptors_.resize(capacity);
        memset(descriptors_.data(), 0, sizeof(rhi::BindingSetItem) * capacity);
        return descriptor_table_ != nullptr;
    }

    void DescriptorTableManager::shutdown() {
        for (auto& descriptor : descriptors_) {
            if (descriptor.resourceHandle) {
                descriptor.resourceHandle->Release();
                descriptor.resourceHandle = nullptr;
            }
        }
        descriptors_.clear();
        descriptor_index_umap_.clear();
        allocated_descriptors_.clear();
        search_start_ = 0;
        descriptor_table_ = nullptr;
        rhi_ = nullptr;
    }

    DescriptorIndex DescriptorTableManager::createDescriptor(rhi::BindingSetItem item) {
        const rhi::BindingSetItem cache_key = item;
        const auto& it = descriptor_index_umap_.find(cache_key);
        if (it != descriptor_index_umap_.end()) { return it->second; }

        ui32 capacity = descriptor_table_->getCapacity();
        bool has_free_slot = false;
        ui32 index = 0;

        for (index = search_start_; index < capacity; index++) {
            if (!allocated_descriptors_[index]) {
                has_free_slot = true;
                break;
            }
        }

        if (!has_free_slot) {
            ui32 new_capacity = ((std::max)(64u, capacity * 2));
            rhi_->getDevice()->resizeDescriptorTable(descriptor_table_, new_capacity);
            allocated_descriptors_.resize(new_capacity);
            descriptors_.resize(new_capacity);

            memset(&descriptors_[capacity], 0, sizeof(rhi::BindingSetItem) * (new_capacity - capacity));

            index = capacity;
            capacity = new_capacity;
        }

        item.slot = index;
        search_start_ = index + 1;
        allocated_descriptors_[index] = true;
        rhi_->getDevice()->writeDescriptorTable(descriptor_table_, item);

        if (item.resourceHandle) { item.resourceHandle->AddRef(); }

        descriptors_[index] = item;
        descriptor_index_umap_.emplace(cache_key, index);

        return index; 
    }
} // dodoe
