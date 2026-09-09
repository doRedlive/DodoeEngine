// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_feature/render_feature.h"
#include "runtime/function/render/render_service/render_target_handle.h"

namespace dodoe {

    class TaaFeature final : public IRenderFeature {
    public:
        void initialize(SharedRenderService& resources) override;
        void onResize(UInt32 width, UInt32 height) override;
        void shutdown() override;

        void registerGraphImports(RenderGraphImportRegistry& imports,
                                  const RenderView& view) override;

        void collectPasses(PassCollector& collector) override;

    private:
        Scope<RenderTargetHandle> m_history_a{};
        Scope<RenderTargetHandle> m_history_b{};
        Scope<RenderTargetHandle> m_prev_depth_a{};
        Scope<RenderTargetHandle> m_prev_depth_b{};
        GfxContext* m_gfx_context{nullptr};
        DeferredDeletionQueue* m_deletion_queue{nullptr};
        UInt64 m_frame_counter{0};
        UInt64 m_history_revision{0};
        UInt64 m_prev_depth_revision{0};
        Bool m_has_prev_frame{false};
        Matrix4f m_prev_unjittered_view_projection{1.0f};
        Vector2f m_prev_jitter_uv{0.0f, 0.0f};
    };

} // namespace dodoe
