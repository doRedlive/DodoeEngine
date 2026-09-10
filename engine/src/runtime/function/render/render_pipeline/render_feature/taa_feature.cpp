// do@Redlive

#include "taa_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_taa_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_taa_depth_copy_pass.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_service/render_target_handle.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/taa_view_extension.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    static RenderTargetDesc BuildTaaHistoryDesc(const String& debug_name) {
        RenderTargetDesc desc{};
        desc.name = debug_name;
        desc.scale_policy = RenderTargetScalePolicy::Relative;
        desc.scale_x = 1.0f;
        desc.scale_y = 1.0f;
        desc.color_attachments.push_back({
            GfxFormat::RGBA16_FLOAT, debug_name, GfxColor(0.0f, 0.0f, 0.0f, 1.0f)
        });
        desc.has_depth = false;
        return desc;
    }

    static RenderTargetDesc BuildTaaPrevDepthDesc(const String& debug_name) {
        RenderTargetDesc desc{};
        desc.name = debug_name;
        desc.scale_policy = RenderTargetScalePolicy::Relative;
        desc.scale_x = 1.0f;
        desc.scale_y = 1.0f;
        desc.color_attachments.push_back({
            GfxFormat::R32_FLOAT, debug_name, GfxColor(1.0f, 1.0f, 1.0f, 1.0f)
        });
        desc.has_depth = false;
        return desc;
    }

    void TaaFeature::initialize(SharedRenderService& resources) {
        auto* gfx = resources.getGfxContext();
        auto* deletion_queue = resources.getRenderTargetSystem()
            ? resources.getRenderTargetSystem()->getDeletionQueue()
            : nullptr;
        DO_ASSERT(gfx != nullptr, "TaaFeature initialize requires valid GfxContext");

        m_gfx_context = gfx;
        m_deletion_queue = deletion_queue;

        m_history_a = create_scope<RenderTargetHandle>();
        m_history_a->initialize(BuildTaaHistoryDesc("TaaHistoryA"), *gfx, deletion_queue);

        m_history_b = create_scope<RenderTargetHandle>();
        m_history_b->initialize(BuildTaaHistoryDesc("TaaHistoryB"), *gfx, deletion_queue);

        m_prev_depth_a = create_scope<RenderTargetHandle>();
        m_prev_depth_a->initialize(BuildTaaPrevDepthDesc("TaaPrevDepthA"), *gfx, deletion_queue);

        m_prev_depth_b = create_scope<RenderTargetHandle>();
        m_prev_depth_b->initialize(BuildTaaPrevDepthDesc("TaaPrevDepthB"), *gfx, deletion_queue);

        m_frame_counter = 0;
        m_history_revision = 0;
        m_prev_depth_revision = 0;
        m_has_prev_frame = false;
        m_prev_unjittered_view_projection = Matrix4f(1.0f);
    }

    void TaaFeature::onResize(const UInt32 width, const UInt32 height) {
        DO_ASSERT(m_gfx_context != nullptr, "TaaFeature onResize requires valid GfxContext");

        if (m_history_a) {
            m_history_a->resolve(width, height, *m_gfx_context, 0);
        }
        if (m_history_b) {
            m_history_b->resolve(width, height, *m_gfx_context, 0);
        }
        if (m_prev_depth_a) {
            m_prev_depth_a->resolve(width, height, *m_gfx_context, 0);
        }
        if (m_prev_depth_b) {
            m_prev_depth_b->resolve(width, height, *m_gfx_context, 0);
        }
    }

    void TaaFeature::shutdown() {
        if (m_history_a) {
            m_history_a->shutdown();
            m_history_a.reset();
        }
        if (m_history_b) {
            m_history_b->shutdown();
            m_history_b.reset();
        }
        if (m_prev_depth_a) {
            m_prev_depth_a->shutdown();
            m_prev_depth_a.reset();
        }
        if (m_prev_depth_b) {
            m_prev_depth_b->shutdown();
            m_prev_depth_b.reset();
        }
        m_gfx_context = nullptr;
        m_deletion_queue = nullptr;
        m_has_prev_frame = false;
    }

    void TaaFeature::registerGraphImports(RenderGraphImportRegistry& imports,
                                          const RenderView& view) {
        DO_ASSERT(m_history_a && m_history_b, "TaaFeature history targets are missing");

        ++m_frame_counter;
        const Bool flip = (m_frame_counter & 1ull) != 0ull;
        auto* history_read = flip ? m_history_b.get() : m_history_a.get();
        auto* history_write = flip ? m_history_a.get() : m_history_b.get();
        auto* prev_depth_read = flip ? m_prev_depth_b.get() : m_prev_depth_a.get();
        auto* prev_depth_write = flip ? m_prev_depth_a.get() : m_prev_depth_b.get();

        const Bool history_changed = history_write->getRevision() != m_history_revision ||
            prev_depth_write->getRevision() != m_prev_depth_revision;
        const Bool reset = !m_has_prev_frame || history_changed;
        m_history_revision = history_write->getRevision();
        m_prev_depth_revision = prev_depth_write->getRevision();

        TaaFrameParams frame_params{};
        frame_params.prev_unjittered_view_projection = m_prev_unjittered_view_projection;
        frame_params.prev_jitter_uv = m_prev_jitter_uv;
        const auto* taa_extension = view.getExtension<TaaViewExtension>();
        if (taa_extension && taa_extension->unjittered_valid) {
            frame_params.current_jitter_uv = Vector2f(
                taa_extension->jitter_ndc.x * 0.5f,
                -taa_extension->jitter_ndc.y * 0.5f);
            frame_params.current_unjittered_view_projection = taa_extension->unjittered_view_projection;
        } else {
            frame_params.current_unjittered_view_projection = view.getViewProjectionMatrix();
        }
        frame_params.reset_history = reset;

        imports.publish<TaaHistoryReadKey>(history_read);
        imports.publish<TaaHistoryWriteKey>(history_write);
        imports.publish<TaaPrevDepthReadKey>(prev_depth_read);
        imports.publish<TaaPrevDepthWriteKey>(prev_depth_write);
        imports.publish<TaaFrameParamsKey>(frame_params);

        if (taa_extension && taa_extension->unjittered_valid) {
            m_prev_unjittered_view_projection = taa_extension->unjittered_view_projection;
            m_prev_jitter_uv = frame_params.current_jitter_uv;
        } else {
            m_prev_unjittered_view_projection = view.getViewProjectionMatrix();
            m_prev_jitter_uv = Vector2f(0.0f, 0.0f);
        }
        m_has_prev_frame = true;
    }

    void TaaFeature::collectPasses(PassCollector& collector) {
        collector.addPass<TaaDepthCopyPass>();
        collector.addPass<TaaPass>();
    }

} // namespace dodoe
