// do@Redlive

#include "binding_set_cache.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"


namespace dodoe {

    namespace {

        Size_t HashLogicalItem(Size_t h, const GfxLogicalBindingItem& item) {
            hash_combine(h, static_cast<Size_t>(item.type));
            hash_combine(h, static_cast<Size_t>(item.slot));
            hash_combine(h, static_cast<Size_t>(item.array_element));
            hash_combine(h, reinterpret_cast<Size_t>(item.texture.get()));
            hash_combine(h, reinterpret_cast<Size_t>(item.buffer.get()));
            hash_combine(h, reinterpret_cast<Size_t>(item.sampler.Get()));
            hash_combine(h, static_cast<Size_t>(item.format));
            hash_combine(h, static_cast<Size_t>(item.dimension));
            hash_combine(h, static_cast<Size_t>(item.raw_data[0]));
            hash_combine(h, static_cast<Size_t>(item.raw_data[1]));
            return h;
        }

    } // namespace

    Bool BindingSetCache::CacheKey::operator==(const CacheKey& other) const {
        if (layout != other.layout || generation != other.generation ||
            desc.bindings.size() != other.desc.bindings.size()) return false;
        for (Size_t i = 0; i < desc.bindings.size(); ++i) {
            const auto& a = desc.bindings[i];
            const auto& b = other.desc.bindings[i];
            if (a.type != b.type || a.slot != b.slot || a.array_element != b.array_element ||
                a.texture.get() != b.texture.get() || a.buffer.get() != b.buffer.get() ||
                a.sampler.Get() != b.sampler.Get() || a.format != b.format || a.dimension != b.dimension ||
                a.raw_data[0] != b.raw_data[0] || a.raw_data[1] != b.raw_data[1]) return false;
        }
        return true;
    }

    Bool BindingSetCache::initialize(const BindingSetCacheCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("BindingSetCache::initialize", "startup");
        m_gfx_context = info.gfx_context;
        DO_ASSERT(m_gfx_context != nullptr, "BindingSetCache requires gfx_context");
        if (!m_gfx_context) {
            DO_ERROR("BindingSetCache::initialize: graphics context is unavailable");
        }
        return m_gfx_context != nullptr;
    }

    void BindingSetCache::shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        DO_PROFILE_SCOPE_CATEGORY("BindingSetCache::shutdown", "shutdown");
        DO_INFO("BindingSetCache: releasing {} cached binding set(s)", m_cache.size());
        m_cache.clear();
        m_gfx_context = nullptr;
    }

    GfxBindingSetHandle BindingSetCache::getOrCreate(DrawCommandList& command_list,
                                                       const GfxLogicalBindingSetDesc& desc,
                                                       GfxBindingLayoutHandle layout,
                                                       UInt64 layout_generation) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Size_t h = reinterpret_cast<Size_t>(layout.Get());
        hash_combine(h, static_cast<Size_t>(layout_generation));
        hash_combine(h, desc.bindings.size());
        for (const auto& item : desc.bindings) {
            h = HashLogicalItem(h, item);
        }

        const CacheKey key{layout, layout_generation, desc, h};
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            if (!it->second) {
                m_cache.erase(it);
            } else if (it->second->isGpuReady()) {
                return it->second;
            } else {
                command_list.requestRealize(it->second);
                return it->second;
            }
        }

        auto binding_set = command_list.createBindingSet(desc, layout);
        if (binding_set) m_cache.emplace(key, binding_set);
        // DO_DEBUG("BindingSetCache: created binding set (cache size={})", m_cache.size());
        return binding_set;
    }

    void BindingSetCache::invalidateFor(const void* resource_ptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        DO_PROFILE_SCOPE_CATEGORY("BindingSetCache::invalidateFor", "resource-cache");
        for (auto it = m_cache.begin(); it != m_cache.end();) {
            Bool references = false;
            if (it->second) {
                const auto& logical_desc = it->second->getLogicalDesc();
                for (const auto& item : logical_desc.bindings) {
                    if (item.texture.get() == resource_ptr || item.buffer.get() == resource_ptr) {
                        references = true;
                        break;
                    }
                }
            }
            if (references) {
                it = m_cache.erase(it);
            } else {
                ++it;
            }
        }
    }

    void BindingSetCache::invalidateForLayout(GfxBindingLayoutHandle layout) {
        std::lock_guard<std::mutex> lock(m_mutex);
        DO_PROFILE_SCOPE_CATEGORY("BindingSetCache::invalidateForLayout", "resource-cache");
        for (auto it = m_cache.begin(); it != m_cache.end();) {
            if (it->first.layout == layout) it = m_cache.erase(it);
            else ++it;
        }
    }

    void BindingSetCache::clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        DO_PROFILE_SCOPE_CATEGORY("BindingSetCache::clear", "resource-cache");
        m_cache.clear();
    }

} // namespace dodoe
