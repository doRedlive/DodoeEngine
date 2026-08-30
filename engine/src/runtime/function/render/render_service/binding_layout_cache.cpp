// do@Redlive

#include "binding_layout_cache.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    namespace {
        Size_t HashBindingLayoutDesc(const GfxBindingLayoutDesc& desc) {
            Size_t h = 0;
            hash_combine(h, static_cast<Size_t>(desc.visibility));
            hash_combine(h, static_cast<Size_t>(desc.registerSpace));
            hash_combine(h, static_cast<Bool>(desc.registerSpaceIsDescriptorSet));
            hash_combine(h, desc.bindingOffsets.shaderResource);
            hash_combine(h, desc.bindingOffsets.sampler);
            hash_combine(h, desc.bindingOffsets.constantBuffer);
            hash_combine(h, desc.bindingOffsets.unorderedAccess);
            hash_combine(h, desc.bindings.size());
            for (Size_t i = 0; i < desc.bindings.size(); ++i) {
                hash_combine(h, static_cast<Size_t>(desc.bindings[i].type));
                hash_combine(h, static_cast<Size_t>(desc.bindings[i].slot));
                hash_combine(h, static_cast<Size_t>(desc.bindings[i].size));
            }
            return h;
        }
    } // anonymous namespace

    Bool BindingLayoutCache::initialize(const BindingLayoutCacheCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("BindingLayoutCache::initialize", "startup");
        m_gfx_context = info.gfx_context;
        DO_ASSERT(m_gfx_context != nullptr, "BindingLayoutCache requires gfx_context");
        if (!m_gfx_context) {
            DO_ERROR("BindingLayoutCache::initialize: graphics context is unavailable");
        }
        return m_gfx_context != nullptr;
    }

    void BindingLayoutCache::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("BindingLayoutCache::shutdown", "shutdown");
        DO_INFO("BindingLayoutCache: releasing {} cached binding layout(s)", m_cache.size());
        m_cache.clear();
        m_gfx_context = nullptr;
    }

    GfxBindingLayoutHandle BindingLayoutCache::getOrCreate(const GfxBindingLayoutDesc& desc) {
        DO_PROFILE_SCOPE_CATEGORY("BindingLayoutCache::getOrCreate", "resource-cache");
        const auto hash = HashBindingLayoutDesc(desc);
        auto it = m_cache.find(hash);
        if (it != m_cache.end()) {
            return it->second.layout;
        }

        auto layout = GDrawCommandList.createBindingLayout(desc);
        m_cache[hash] = {layout, m_next_generation++};
        // DO_DEBUG("BindingLayoutCache: created binding layout (bindings={}, generation={})",
        //     desc.bindings.size(), m_cache[hash].generation);
        return layout;
    }

    UInt64 BindingLayoutCache::getLayoutGeneration(GfxBindingLayoutHandle layout) const {
        for (const auto& [hash, entry] : m_cache) {
            if (entry.layout == layout) {
                return entry.generation;
            }
        }
        return 0;
    }

    void BindingLayoutCache::clear() {
        DO_PROFILE_SCOPE_CATEGORY("BindingLayoutCache::clear", "resource-cache");
        m_cache.clear();
        m_next_generation = 1;
    }

} // namespace dodoe
