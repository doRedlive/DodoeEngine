// do@Redlive

#include "descriptor_table_manager.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_settings.h"

namespace dodoe {

    bool DescriptorTableManager::initialize(const DescriptorTableManagerCreateInfo& info) {
        gfx_ = info.gfx;

        if (!RenderSettings::IsBindlessActive()) {
            return gfx_ != nullptr;
        }

        GfxBindlessLayoutDesc bindless_layout_desc;
        bindless_layout_desc.visibility = GfxShaderType::All;
        bindless_layout_desc.firstSlot = 0;
        bindless_layout_desc.maxCapacity = 1024;
        bindless_layout_desc.registerSpaces = {
            GfxBindingLayoutItem::Texture_SRV(0)
        };
        auto bindless_layout = GDrawCommandList.getDevice()->createBindlessLayout(bindless_layout_desc);
        descriptor_table_ = GDrawCommandList.getDevice()->createDescriptorTable(bindless_layout);

        size_t capacity = descriptor_table_->getCapacity();
        allocated_descriptors_.resize(capacity);
        descriptors_.resize(capacity);
        memset(descriptors_.data(), 0, sizeof(GfxBindingSetItem) * capacity);
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

    DescriptorIndex DescriptorTableManager::createDescriptor(GfxBindingSetItem item) {
        if (!descriptor_table_) { return -1; }

        const GfxBindingSetItem cache_key = item;
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
            GDrawCommandList.getDevice()->resizeDescriptorTable(descriptor_table_, new_capacity);
            allocated_descriptors_.resize(new_capacity);
            descriptors_.resize(new_capacity);

            memset(&descriptors_[capacity], 0, sizeof(GfxBindingSetItem) * (new_capacity - capacity));

            index = capacity;
            capacity = new_capacity;
        }

        item.slot = index;
        search_start_ = index + 1;
        allocated_descriptors_[index] = true;

        GDrawCommandList.getDevice()->writeDescriptorTable(descriptor_table_, item);

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

    UInt32 DescriptorTableManager::allocateSlot() {
        if (!descriptor_table_) { return 0; }

        ui32 capacity = descriptor_table_->getCapacity();
        ui32 index = 0;

        for (index = search_start_; index < capacity; index++) {
            if (!allocated_descriptors_[index]) {
                allocated_descriptors_[index] = true;
                search_start_ = index + 1;
                return index;
            }
        }

        ui32 new_capacity = ((std::max)(64u, capacity * 2));
        GDrawCommandList.getDevice()->resizeDescriptorTable(descriptor_table_, new_capacity);
        allocated_descriptors_.resize(new_capacity);
        descriptors_.resize(new_capacity);
        memset(&descriptors_[capacity], 0, sizeof(GfxBindingSetItem) * (new_capacity - capacity));

        index = capacity;
        allocated_descriptors_[index] = true;
        search_start_ = index + 1;
        return index;
    }

} // dodoe
