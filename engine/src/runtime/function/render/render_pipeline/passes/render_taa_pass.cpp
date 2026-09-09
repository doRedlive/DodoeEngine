// do@Redlive

#include "render_taa_pass.h"

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
#include "runtime/core/math/math.h"

#include <cstring>

namespace dodoe {

    namespace {
        constexpr UInt64 kTaaConstantBufferSize = sizeof(Matrix4f) * 2 + sizeof(Vector4f) * 3;

        struct TaaConstantsData {
            Matrix4f prev_view_projection{1.0f};
            Matrix4f current_view_projection{1.0f};
            Vector4f params{0.0f, 0.0f, 0.0f, 0.9f};
            Vector4f prev_params{0.0f, 0.0f, 0.0f, 0.0f};
            Vector4f tuning{1.5f, 0.01f, 16.0f, 0.2f};
        };

        static_assert(sizeof(TaaConstantsData) == kTaaConstantBufferSize);
        static_assert(kTaaConstantBufferSize <= 256);
    }

    struct TaaPassParameters {
        RenderGraphTextureHandle input{};
        RenderGraphTextureHandle history{};
        RenderGraphTextureHandle position{};
        RenderGraphTextureHandle motion_vector{};
        RenderGraphTextureHandle prev_depth{};
        RenderGraphTextureHandle output{};
    };

    void TaaPass::build(RenderGraphBuilder& graph,
                        const RenderPassBuildContext& context) {
        const auto* shader_library = context.shared_render_service->getShaderLibrary();
        auto* binding_layout_cache = context.shared_render_service->getBindingLayoutCache();
        DO_ASSERT(shader_library != nullptr, "TaaPass shader library is null");
        DO_ASSERT(binding_layout_cache != nullptr, "TaaPass binding layout cache is null");
        DO_ASSERT(context.graph_imports != nullptr, "TaaPass graph imports are null");

        const auto binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Pixel)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::ConstantBuffer(0))
                .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                .addItem(GfxBindingLayoutItem::Texture_SRV(2))
                .addItem(GfxBindingLayoutItem::Texture_SRV(3))
                .addItem(GfxBindingLayoutItem::Texture_SRV(4))
                .addItem(GfxBindingLayoutItem::Texture_SRV(5))
                .addItem(GfxBindingLayoutItem::Sampler(9)));

        const TaaFrameParams frame_params = context.graph_imports->require<TaaFrameParamsKey>();

        graph.addPass<TaaPassParameters>(
            "TaaPass",
            RenderGraphPassFlags::Raster,
            [&context](RenderGraphPassBuilder& pass_builder, TaaPassParameters& parameters) {
                const auto* hdr = pass_builder.blackboard().get<SceneHdrKey>();
                DO_ASSERT(hdr, "TaaPass hdr input is missing");
                parameters.input = pass_builder.read(*hdr);

                const auto* scene_textures = pass_builder.blackboard().get<SceneTexturesKey>();
                DO_ASSERT(scene_textures, "TaaPass scene textures are missing");
                parameters.position = pass_builder.read(scene_textures->position);
                parameters.motion_vector = pass_builder.read(scene_textures->motion_vector);

                auto* history_read = context.graph_imports->require<TaaHistoryReadKey>();
                auto* history_write = context.graph_imports->require<TaaHistoryWriteKey>();
                auto* prev_depth_read = context.graph_imports->require<TaaPrevDepthReadKey>();
                DO_ASSERT(history_read != nullptr, "TaaPass history read target is missing");
                DO_ASSERT(history_write != nullptr, "TaaPass history write target is missing");
                DO_ASSERT(prev_depth_read != nullptr, "TaaPass prev depth read target is missing");

                parameters.history = pass_builder.read(pass_builder.importTexture(
                    history_read->getColorTexture(), "TaaHistoryRead"));

                parameters.prev_depth = pass_builder.read(pass_builder.importTexture(
                    prev_depth_read->getColorTexture(), "TaaPrevDepthRead"));

                RenderGraphAttachmentInfo output_attachment{};
                output_attachment.load_op = LoadOp::DontCare;
                parameters.output = pass_builder.writeColor(pass_builder.importTexture(
                    history_write->getColorTexture(), "TaaHistoryWrite"), output_attachment);

                pass_builder.blackboard().set<SceneHdrKey>(parameters.output);
            },
            [frame_params, shader_library, binding_layout](const TaaPassParameters& parameters,
                                                           const RenderGraphPassContext& ctx,
                                                           DrawCommandList& command_list) {
                auto* staging = ctx.getFrameStagingAllocator();
                if (!staging) {
                    DO_ERROR("TaaPass: frame staging allocator is null");
                    return;
                }

                const auto input_handle = ctx.resolveTexture(parameters.input);
                const auto history_handle = ctx.resolveTexture(parameters.history);
                const auto position_handle = ctx.resolveTexture(parameters.position);
                const auto motion_handle = ctx.resolveTexture(parameters.motion_vector);
                const auto prev_depth_handle = ctx.resolveTexture(parameters.prev_depth);
                const auto output_handle = ctx.resolveTexture(parameters.output);
                if (!input_handle || !history_handle || !position_handle || !motion_handle ||
                    !prev_depth_handle || !output_handle) {
                    DO_ERROR("TaaPass: failed to resolve graph textures");
                    return;
                }

                TaaConstantsData constants{};
                constants.prev_view_projection = frame_params.prev_unjittered_view_projection;
                constants.current_view_projection = frame_params.current_unjittered_view_projection;
                constants.params = Vector4f(
                    frame_params.current_jitter_uv.x,
                    frame_params.current_jitter_uv.y,
                    frame_params.reset_history ? 1.0f : 0.0f,
                    frame_params.reset_history ? 0.0f : 0.9f);
                constants.prev_params = Vector4f(
                    frame_params.prev_jitter_uv.x,
                    frame_params.prev_jitter_uv.y,
                    0.0f, 0.0f);
                constants.tuning = Vector4f(1.5f, 0.01f, 16.0f, 0.2f);

                const auto allocation = staging->allocate(kTaaConstantBufferSize);
                if (!allocation.buffer || !allocation.mapped_data) {
                    DO_ERROR("TaaPass: unable to allocate constant buffer");
                    return;
                }
                std::memset(allocation.mapped_data, 0, static_cast<Size_t>(allocation.size));
                std::memcpy(allocation.mapped_data, &constants, sizeof(constants));

                const auto binding_set = command_list.createBindingSet(
                    GfxBindingSetDesc()
                        .addItem(GfxBindingSetItem::ConstantBuffer(
                            0, allocation.buffer->getRHIHandle().Get(),
                            GfxBufferRange(allocation.offset, allocation.size)))
                        .addItem(GfxBindingSetItem::Texture_SRV(1, input_handle->getRHIHandle().Get()))
                        .addItem(GfxBindingSetItem::Texture_SRV(2, history_handle->getRHIHandle().Get()))
                        .addItem(GfxBindingSetItem::Texture_SRV(3, position_handle->getRHIHandle().Get()))
                        .addItem(GfxBindingSetItem::Texture_SRV(4, motion_handle->getRHIHandle().Get()))
                        .addItem(GfxBindingSetItem::Texture_SRV(5, prev_depth_handle->getRHIHandle().Get()))
                        .addItem(GfxBindingSetItem::Sampler(9, GlobalSamplers::Screen().Get())),
                    binding_layout);
                if (!binding_set) {
                    DO_ERROR("TaaPass: failed to create binding set");
                    return;
                }

                const auto pipeline = ctx.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        shader_library->getFullscreenVertexShader(),
                        shader_library->getTaaPixelShader(),
                        binding_layout),
                    ctx.getRenderTargetSignature(),
                    command_list);
                if (!pipeline) {
                    DO_ERROR("TaaPass: failed to create pipeline");
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
