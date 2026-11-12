// do@Redlive
#pragma once

#include "render_feature.h"
#include "../render_pipeline_pass_utils.h"
#include "../passes/render_pass_blackboard_keys.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe {
    // A 2D view always has a defined background, even with all drawing features disabled.
    class SceneColorClearPass final : public IRenderPass {
    public:
        using Produces = TypeList<SceneColorKey>;
        RenderPhase getPhase() const override { return RenderPhase::Opaque; }
        DynamicArray<Size_t> getProducedKeys() const override { return MakeKeyHashes(Produces{}); }
        void build(RenderGraphBuilder& graph, const RenderPassBuildContext& context) override {
            struct Parameters {};
            graph.addPass<Parameters>("SceneColorClear", RenderGraphPassFlags::Raster,
                [&context](RenderGraphPassBuilder& builder, Parameters&) {
                    const auto background = context.view.getBackground();
                    RenderGraphAttachmentInfo attachment{};
                    attachment.load_op = LoadOp::Clear;
                    attachment.clear_color = GfxColor(background.x, background.y, background.z, background.w);
                    const auto color = builder.writeColor(builder.createTransientTexture(
                        rendering_pipeline_utils::MakeSwapchainRT2D(context.gfx_context->getSwapchainExtent2D(),
                            GfxFormat::RGBA8_UNORM, "RDG SceneColor"), "SceneColor"), attachment);
                    builder.blackboard().set<SceneColorKey>(color);
                }, [](const Parameters&, const RenderGraphPassContext&, DrawCommandList&) {});
        }
    };

    class SceneColorFeature final : public IRenderFeature {
    public:
        void collectPasses(PassCollector& collector) override { collector.addPass<SceneColorClearPass>(); }
    };
}
