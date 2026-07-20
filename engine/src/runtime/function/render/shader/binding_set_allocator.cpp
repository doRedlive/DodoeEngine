// do@Redlive

#include "binding_set_allocator.h"

#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    GfxBindingSetHandle BindingSetAllocator::getOrCreate(const BindingSetCacheKey& key,
                                                          GfxDeviceHandle device) {
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            return it->second;
        }

        GfxBindingSetDesc desc;
        for (const auto& [slot, item] : key.items) {
            desc.addItem(item);
        }

        auto binding_set = device->createBindingSet(desc, key.layout);
        m_cache[key] = binding_set;
        return binding_set;
    }

    GfxBindingSetHandle BindingSetAllocator::getOrCreate(const BindingSetCacheKey& key,
                                                          DrawCommandList& cmd_list) {
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            return it->second;
        }

        GfxBindingSetDesc desc;
        for (const auto& [slot, item] : key.items) {
            desc.addItem(item);
        }

        auto binding_set = cmd_list.createBindingSet(desc, key.layout);
        m_cache[key] = binding_set;
        return binding_set;
    }

    void BindingSetAllocator::clear() {
        m_cache.clear();
    }

} // namespace dodoe
