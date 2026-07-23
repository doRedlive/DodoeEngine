// do@Redlive

#pragma once

#include "dopch.h"

#include "render_target_handle.h"

namespace dodoe {

    class GfxContext;

    class RenderTargetSystem {
    public:
        RenderTargetSystem() = default;

        void initialize(GfxContext& gfx, DeferredDeletionQueue* deletion_queue);
        void shutdown();

        RenderTargetHandle* create(const String& name, const RenderTargetDesc& desc);

        [[nodiscard]] RenderTargetHandle* find(const String& name);
        [[nodiscard]] const RenderTargetHandle* find(const String& name) const;

        void onResize(UInt32 width, UInt32 height, UInt64 current_frame);

        [[nodiscard]] DeferredDeletionQueue* getDeletionQueue() const { return m_deletion_queue; }
        [[nodiscard]] GfxContext* getGfxContext() const { return m_gfx_context; }

    private:
        UnorderedMap<String, Scope<RenderTargetHandle>> m_handles{};
        DeferredDeletionQueue* m_deletion_queue{nullptr};
        GfxContext* m_gfx_context{nullptr};
    };

} // namespace dodoe
