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
        Bool EqualBindingLayout(const GfxBindingLayoutDesc& a, const GfxBindingLayoutDesc& b) {
            if (a.visibility != b.visibility || a.registerSpace != b.registerSpace ||
                a.registerSpaceIsDescriptorSet != b.registerSpaceIsDescriptorSet ||
                a.bindingOffsets.shaderResource != b.bindingOffsets.shaderResource ||
                a.bindingOffsets.sampler != b.bindingOffsets.sampler ||
                a.bindingOffsets.constantBuffer != b.bindingOffsets.constantBuffer ||
                a.bindingOffsets.unorderedAccess != b.bindingOffsets.unorderedAccess ||
                a.bindings.size() != b.bindings.size()) return false;
            for (Size_t i = 0; i < a.bindings.size(); ++i)
                if (a.bindings[i].type != b.bindings[i].type || a.bindings[i].slot != b.bindings[i].slot ||
                    a.bindings[i].size != b.bindings[i].size) return false;
            return true;
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
        std::lock_guard<std::mutex> lock(m_mutex);
        DO_PROFILE_SCOPE_CATEGORY("BindingLayoutCache::shutdown", "shutdown");
        DO_INFO("BindingLayoutCache: releasing {} cached binding layout(s)", m_cache.size());
        m_cache.clear();
        m_gfx_context = nullptr;
    }

    GfxBindingLayoutHandle BindingLayoutCache::getOrCreate(const GfxBindingLayoutDesc& desc) {
        DO_PROFILE_SCOPE_CATEGORY("BindingLayoutCache::getOrCreate", "resource-cache");
        const auto hash = HashBindingLayoutDesc(desc);
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& entry : m_cache)
            if (entry.hash == hash && EqualBindingLayout(entry.desc, desc)) return entry.layout;
        auto layout = m_gfx_context->getDevice()->createBindingLayout(desc);
        if (layout) m_cache.push_back({layout, m_next_generation++, desc, hash});
        return layout;
    }

    UInt64 BindingLayoutCache::getLayoutGeneration(GfxBindingLayoutHandle layout) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& entry : m_cache) {
            if (entry.layout == layout) {
                return entry.generation;
            }
        }
        return 0;
    }

    void BindingLayoutCache::clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        DO_PROFILE_SCOPE_CATEGORY("BindingLayoutCache::clear", "resource-cache");
        m_cache.clear();
    }

} // namespace dodoe
