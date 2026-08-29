// do@Redlive

#include "render_target_system.h"

#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    Bool RenderTargetSystem::initialize(const RenderTargetSystemCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("RenderTargetSystem::initialize", "startup");
        m_gfx_context = info.gfx_context;
        m_deletion_queue = info.deletion_queue;
        if (!m_gfx_context || !m_deletion_queue) {
            DO_ERROR("RenderTargetSystem::initialize: graphics context or deletion queue is unavailable");
        }
        return m_gfx_context != nullptr && m_deletion_queue != nullptr;
    }

    void RenderTargetSystem::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("RenderTargetSystem::shutdown", "shutdown");
        DO_INFO("RenderTargetSystem: releasing {} render target(s)", m_handles.size());
        for (auto& [name, handle] : m_handles) {
            if (handle) {
                handle->shutdown();
            }
        }
        m_handles.clear();
        m_gfx_context = nullptr;
        m_deletion_queue = nullptr;
    }

    RenderTargetHandle* RenderTargetSystem::create(const String& name,
                                                     const RenderTargetDesc& desc) {
        DO_PROFILE_SCOPE_CATEGORY("RenderTargetSystem::create", "resource");
        auto it = m_handles.find(name);
        if (it != m_handles.end()) {
            const auto& existing = it->second->getDesc();
            if (existing.scale_policy != desc.scale_policy ||
                existing.scale_x != desc.scale_x ||
                existing.scale_y != desc.scale_y ||
                existing.fixed_width != desc.fixed_width ||
                existing.fixed_height != desc.fixed_height ||
                existing.sample_count != desc.sample_count ||
                existing.has_depth != desc.has_depth ||
                existing.depth_format != desc.depth_format ||
                existing.color_attachments.size() != desc.color_attachments.size()) {
                DO_ERROR("RenderTargetSystem::create duplicate name with different desc: {}", name);
                return nullptr;
            }
            for (Size_t i = 0; i < existing.color_attachments.size(); ++i) {
                if (existing.color_attachments[i].format != desc.color_attachments[i].format) {
                    DO_ERROR("RenderTargetSystem::create duplicate name with different color format: {}", name);
                    return nullptr;
                }
            }
            return it->second.get();
        }

        DO_ASSERT(m_gfx_context != nullptr, "RenderTargetSystem not initialized");

        auto handle = create_scope<RenderTargetHandle>();
        handle->initialize(desc, *m_gfx_context, m_deletion_queue);
        auto* ptr = handle.get();
        m_handles[name] = std::move(handle);
        DO_INFO("RenderTargetSystem: created target '{}' (attachments={}, depth={}, samples={})",
            name, desc.color_attachments.size(), desc.has_depth, desc.sample_count);
        return ptr;
    }

    RenderTargetHandle* RenderTargetSystem::find(const String& name) {
        auto it = m_handles.find(name);
        if (it != m_handles.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    const RenderTargetHandle* RenderTargetSystem::find(const String& name) const {
        auto it = m_handles.find(name);
        if (it != m_handles.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    void RenderTargetSystem::onResize(const UInt32 width, const UInt32 height,
                                       const UInt64 current_frame) {
        DO_PROFILE_SCOPE_CATEGORY("RenderTargetSystem::onResize", "swapchain");
        DO_ASSERT(m_gfx_context != nullptr, "RenderTargetSystem not initialized");

        for (auto& [name, handle] : m_handles) {
            if (handle) {
                handle->resolve(width, height, *m_gfx_context, current_frame);
            }
        }
    }

} // namespace dodoe
