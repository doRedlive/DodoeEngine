// do@Redlive

#include "render_test_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/pipeline/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/function/render/texture/texture_manager.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/binding_set_cache.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "render_pass_blackboard_keys.h"

namespace dodoe {

    struct TestTriangleVertex {
        Float px, py, pz;
        Float r, g, b, a;
        Float u, v;
    };

    inline constexpr TestTriangleVertex kTestQuadVertices[] = {
        {-1.0f, -1.0f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f},
        { 1.0f, -1.0f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f},
        {-1.0f,  1.0f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f},
        { 1.0f,  1.0f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f},
        {-1.0f,  1.0f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f},
    };
    inline constexpr UInt32 kTestQuadVertexCount = static_cast<UInt32>(std::size(kTestQuadVertices));

    struct TestPassParameters {
        RenderGraphTextureHandle color_target{};
        RenderGraphBufferHandle triangle_vb{};
    };

    void TestPass::build(RenderGraphBuilder& graph,
                          const RenderPassBuildContext& context) {
        graph.addPass<TestPassParameters>(
            "TestTrianglePass",
            RenderGraphPassFlags::Raster,
            [&context](RenderGraphPassBuilder& pass_builder, TestPassParameters& parameters) {
                const auto* scene_color = pass_builder.blackboard().get<SceneColorKey, RenderGraphTextureHandle>();
                if (scene_color) {
                    parameters.color_target = pass_builder.writeColor(*scene_color);
                } else {
                    const auto swapchain_extent = context.gfx_context->getSwapchainExtent2d();
                    parameters.color_target = pass_builder.writeColor(pass_builder.createTransientTexture(
                        rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG TestColor"),
                        "TestColor"));
                    pass_builder.blackboard().set<SceneColorKey>(parameters.color_target);
                }

                RenderGraphBufferDesc tri_vb_desc{};
                tri_vb_desc.desc = GfxBufferDesc()
                    .setByteSize(static_cast<UInt32>(sizeof(kTestQuadVertices)))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG TestQuadVB");
                parameters.triangle_vb = pass_builder.writeBuffer(
                    pass_builder.createTransientBuffer(tri_vb_desc, "TestTriangleVB"),
                    RenderGraphPipelineStage::Copy);
            },
            [](const TestPassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                const auto color_target = ctx.resolveTexture(parameters.color_target);
                const auto quad_vb = ctx.resolveBuffer(parameters.triangle_vb);

                command_list.setBufferState(quad_vb, GfxResourceStates::CopyDest);
                command_list.commitBarriers();
                command_list.writeBuffer(quad_vb, kTestQuadVertices, sizeof(kTestQuadVertices), 0);
                command_list.setBufferState(quad_vb, GfxResourceStates::VertexBuffer);
                command_list.commitBarriers();

                auto* tm = ctx.getTextureManager();
                Texture2D* loaded_tex = nullptr;
                GfxTextureHandle test_tex;
                if (tm) {
                    loaded_tex = tm->loadTexture("engine/res/pictures/grm.jpg", command_list);
                    if (loaded_tex && loaded_tex->getGpuHandle()) {
                        test_tex = loaded_tex->getGpuHandle();
                    }
                }
                if (!test_tex) {
                    DO_ERROR("TestPass: failed to load grm.jpg");
                    return;
                }

                auto* shared_service = ctx.getSharedRenderService();
                auto* binding_layout_cache = shared_service ? shared_service->getBindingLayoutCache() : nullptr;
                auto* binding_set_cache = shared_service ? shared_service->getBindingSetCache() : nullptr;
                auto* input_layout_cache = shared_service ? shared_service->getInputLayoutCache() : nullptr;
                if (!binding_layout_cache || !binding_set_cache || !input_layout_cache) {
                    DO_ERROR("TestPass: render resource caches are unavailable");
                    return;
                }

                const auto binding_layout = binding_layout_cache->getOrCreate(
                    GfxBindingLayoutDesc()
                        .setVisibility(GfxShaderType::Pixel)
                        .addItem(GfxBindingLayoutItem::Texture_SRV(0))
                        .addItem(GfxBindingLayoutItem::Sampler(0)));

                auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(color_target);
                auto fb = command_list.createFramebuffer(framebuffer_desc);

                const auto* shader_library = ctx.getShaderLibrary();
                if (!shader_library) {
                    DO_ERROR("TestPass: shader_library is null");
                    return;
                }

                const auto test_vs = shader_library->getTestVertexShader();
                const auto test_ps = shader_library->getTestPixelShader();
                if (!test_vs || !test_ps) {
                    DO_ERROR("TestPass: test shaders not loaded");
                    return;
                }

                constexpr UInt32 kVertexStride = sizeof(TestTriangleVertex);
                const DynamicArray<GfxVertexAttributeDesc> attribs = {
                    GfxVertexAttributeDesc().setName("POSITION").setFormat(GfxFormat::RGB32_FLOAT).setOffset(0).setElementStride(kVertexStride),
                    GfxVertexAttributeDesc().setName("COLOR").setFormat(GfxFormat::RGBA32_FLOAT).setOffset(12).setElementStride(kVertexStride),
                    GfxVertexAttributeDesc().setName("TEXCOORD").setFormat(GfxFormat::RG32_FLOAT).setOffset(28).setElementStride(kVertexStride),
                };
                const auto input_layout = input_layout_cache->getOrCreate(attribs, test_vs);
                const auto binding_set = binding_set_cache->getOrCreate(
                    GfxBindingSetDesc()
                        .addItem(GfxBindingSetItem::Texture_SRV(0, test_tex->getRHIHandle()))
                        .addItem(GfxBindingSetItem::Sampler(0, GlobalSamplers::screen().Get())),
                    binding_layout,
                    binding_layout_cache->getLayoutGeneration(binding_layout));

                GfxDepthStencilState ds;
                ds.disableDepthTest().disableDepthWrite().disableStencil();
                GfxRasterState raster;
                raster.setCullNone();
                GfxRenderState render_state;
                render_state.setDepthStencilState(ds).setRasterState(raster);

                auto pipeline_desc = GfxGraphicsPipelineDesc()
                    .setVertexShader(test_vs)
                    .setPixelShader(test_ps)
                    .setInputLayout(input_layout)
                    .addBindingLayout(binding_layout)
                    .setPrimType(GfxPrimitiveType::TriangleList)
                    .setRenderState(render_state);

                auto* pipeline_cache = ctx.getPipelineStateCache();
                if (!pipeline_cache) {
                    DO_ERROR("TestPass: pipeline_cache is null");
                    return;
                }

                GfxFramebufferInfo framebuffer_info(framebuffer_desc);
                auto pipeline = pipeline_cache->resolveGraphicsPipeline(pipeline_desc, framebuffer_info, command_list);
                if (!pipeline) {
                    DO_ERROR("TestPass: failed to create pipeline");
                    return;
                }

                const auto swapchain_extent = ctx.getGfxContext()->getSwapchainExtent2d();
                auto vp = GfxViewportState().addViewportAndScissorRect(GfxViewport(
                    0, static_cast<float>(swapchain_extent.x),
                    0, static_cast<float>(swapchain_extent.y),
                    0, 1));

                command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.commitBarriers();

                DynamicArray<GfxBindingSetHandle> bs_arr = {binding_set};
                DynamicArray<GfxVertexBufferBinding> vbs;
                vbs.push_back(GfxVertexBufferBinding()
                    .setBuffer(quad_vb->getRHIHandle()).setSlot(0).setOffset(0));

                command_list.setGraphicsState(fb, pipeline, bs_arr, vp, vbs);
                command_list.draw(GfxDrawArguments().setVertexCount(kTestQuadVertexCount).setInstanceCount(1));

                command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            }
        );
    }

} // namespace dodoe
