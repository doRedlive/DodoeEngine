// do@Redlive

#include "render_taa_depth_copy_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "runtime/function/render/render_pipeline/passes/render_pass_blackboard_keys.h"

#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_pipeline/render_graph_import_registry.h"

#include "runtime/function/render/render_frame/frame_staging_allocator.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/pipeline_state/pipeline_state_cache.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"

namespace dodoe {

    struct TaaDepthCopyPassParameters {
        RenderGraphTextureHandle depth{};
        RenderGraphTextureHandle output{};
    };

    void TaaDepthCopyPass::build(RenderGraphBuilder& graph,
                                 const RenderPassBuildContext& context) {
        const auto* shader_library = context.shared_render_service->getShaderLibrary();
        auto* binding_layout_cache = context.shared_render_service->getBindingLayoutCache();
        DO_ASSERT(shader_library != nullptr, "TaaDepthCopyPass shader library is null");
        DO_ASSERT(binding_layout_cache != nullptr, "TaaDepthCopyPass binding layout cache is null");
        DO_ASSERT(context.graph_imports != nullptr, "TaaDepthCopyPass graph imports are null");

        const auto binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Pixel)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                .addItem(GfxBindingLayoutItem::Sampler(9)));

        graph.addPass<TaaDepthCopyPassParameters>(
            "TaaDepthCopyPass",
            RenderGraphPassFlags::Raster,
            [&context](RenderGraphPassBuilder& pass_builder, TaaDepthCopyPassParameters& parameters) {
                const auto* scene_textures = pass_builder.blackboard().get<SceneTexturesKey>();
                DO_ASSERT(scene_textures, "TaaDepthCopyPass scene textures are missing");
                parameters.depth = pass_builder.read(scene_textures->depth);

                auto* prev_depth_write = context.graph_imports->require<TaaPrevDepthWriteKey>();
                DO_ASSERT(prev_depth_write != nullptr, "TaaDepthCopyPass prev depth write target is missing");

                RenderGraphAttachmentInfo output_attachment{};
                output_attachment.load_op = LoadOp::DontCare;
                parameters.output = pass_builder.writeColor(pass_builder.importTexture(
                    prev_depth_write->getColorTexture(), "TaaPrevDepthWrite"), output_attachment);
            },
            [shader_library, binding_layout](const TaaDepthCopyPassParameters& parameters,
                                             const RenderGraphPassContext& ctx,
                                             DrawCommandList& command_list) {
                const auto depth_handle = ctx.resolveTexture(parameters.depth);
                const auto output_handle = ctx.resolveTexture(parameters.output);
                if (!depth_handle || !output_handle) {
                    DO_ERROR("TaaDepthCopyPass: failed to resolve graph textures");
                    return;
                }

                const auto binding_set = command_list.createBindingSet(
                    GfxBindingSetDesc()
                        .addItem(GfxBindingSetItem::Texture_SRV(1, depth_handle->getRHIHandle().Get()))
                        .addItem(GfxBindingSetItem::Sampler(9, GlobalSamplers::Screen().Get())),
                    binding_layout);
                if (!binding_set) {
                    DO_ERROR("TaaDepthCopyPass: failed to create binding set");
                    return;
                }

                const auto pipeline = ctx.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        shader_library->getFullscreenVertexShader(),
                        shader_library->getTaaDepthCopyPixelShader(),
                        binding_layout),
                    ctx.getRenderTargetSignature(),
                    command_list);
                if (!pipeline) {
                    DO_ERROR("TaaDepthCopyPass: failed to create pipeline");
                    return;
                }

                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(
                    *ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2D());

                DynamicArray<GfxBindingSetHandle> binding_sets = {binding_set};
                command_list.setGraphicsState(ctx.getFramebuffer(), pipeline, binding_sets, viewport_state);
                command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
            });
    }

} // namespace dodoe
