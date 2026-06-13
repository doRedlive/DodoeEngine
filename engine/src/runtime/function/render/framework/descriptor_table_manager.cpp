// do@Redlive

#include "descriptor_table_manager.h"

#include "../interface/gfx_context.h"

namespace dodoe {

    bool DescriptorTableManager::initialize(const DescriptorTableManagerCreateInfo& info) {
        gfx_ = info.gfx;

        gfx::BindlessLayoutDesc bindless_layout_desc;
        bindless_layout_desc.visibility = gfx::ShaderType::All;
        bindless_layout_desc.firstSlot = 0;
        bindless_layout_desc.maxCapacity = 1024;
        bindless_layout_desc.registerSpaces = {
            gfx::BindingLayoutItem::Texture_SRV(0)
        };
        auto bindless_layout = gfx_->getDevice()->createBindlessLayout(bindless_layout_desc);
        descriptor_table_ = gfx_->getDevice()->createDescriptorTable(bindless_layout);

        size_t capacity = descriptor_table_->getCapacity();
        allocated_descriptors_.resize(capacity);
        descriptors_.resize(capacity);
        memset(descriptors_.data(), 0, sizeof(gfx::BindingSetItem) * capacity);
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
        gfx_ = nullptr;
    }

    DescriptorIndex DescriptorTableManager::createDescriptor(gfx::BindingSetItem item) {
        const gfx::BindingSetItem cache_key = item;
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
            gfx_->getDevice()->resizeDescriptorTable(descriptor_table_, new_capacity);
            allocated_descriptors_.resize(new_capacity);
            descriptors_.resize(new_capacity);

            memset(&descriptors_[capacity], 0, sizeof(gfx::BindingSetItem) * (new_capacity - capacity));

            index = capacity;
            capacity = new_capacity;
        }

        item.slot = index;
        search_start_ = index + 1;
        allocated_descriptors_[index] = true;
        gfx_->getDevice()->writeDescriptorTable(descriptor_table_, item);

        if (item.resourceHandle) { item.resourceHandle->AddRef(); }

        descriptors_[index] = item;
        descriptor_index_umap_.emplace(cache_key, index);

        return index;
    }

    void DescriptorTableManager::releaseDescriptor(const DescriptorIndex index) {
        if (index < 0 || index >= static_cast<Int32>(allocated_descriptors_.size())) {
            return;
        }
        if (!allocated_descriptors_[index]) {
            return;
        }

        allocated_descriptors_[index] = false;

        if (descriptors_[index].resourceHandle) {
            descriptors_[index].resourceHandle->Release();
        }

        for (auto it = descriptor_index_umap_.begin(); it != descriptor_index_umap_.end(); ++it) {
            if (it->second == index) {
                descriptor_index_umap_.erase(it);
                break;
            }
        }

        if (index < search_start_) {
            search_start_ = index;
        }
    }

} // dodoe
