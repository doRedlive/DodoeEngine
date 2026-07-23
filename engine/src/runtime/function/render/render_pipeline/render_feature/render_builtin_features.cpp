// do@Redlive

#include "render_builtin_features.h"

#include "runtime/function/render/render_pipeline/passes/render_base_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_deferred_light_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_skybox_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_post_process_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_post_process_2d_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_present_pass.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/shared_render_service.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"

namespace dodoe {

    static constexpr UInt64 kDeferredFrameDelay = 3;

    static RenderTargetDesc BuildGBufferDesc() {
        RenderTargetDesc desc{};
        desc.name = "GBuffer";
        desc.scale_policy = RenderTargetScalePolicy::Relative;
        desc.scale_x = 1.0f;
        desc.scale_y = 1.0f;

        desc.color_attachments.push_back({
            GfxFormat::RGBA8_UNORM, "GBufferAlbedo", GfxColor(0.08f, 0.09f, 0.11f, 1.0f)
        });
        desc.color_attachments.push_back({
            GfxFormat::RGBA16_FLOAT, "GBufferNormal", GfxColor(0.0f, 0.0f, 0.0f, 1.0f)
        });
        desc.color_attachments.push_back({
            GfxFormat::RGBA32_FLOAT, "GBufferPosition", GfxColor(0.0f, 0.0f, 0.0f, 1.0f)
        });
        desc.color_attachments.push_back({
            GfxFormat::RGBA8_UNORM, "GBufferMaterial", GfxColor(0.0f, 1.0f, 1.0f, 1.0f)
        });

        desc.has_depth = true;
        desc.depth_format = GfxFormat::D32;
        desc.depth_debug_name = "GBufferDepth";
        desc.clear_depth = 1.0f;

        return desc;
    }

    static RenderTargetDesc BuildShadowMapDesc() {
        RenderTargetDesc desc{};
        desc.name = "ShadowMap";
        desc.scale_policy = RenderTargetScalePolicy::Relative;
        desc.scale_x = 1.0f;
        desc.scale_y = 1.0f;

        desc.has_depth = true;
        desc.depth_format = GfxFormat::D32;
        desc.depth_debug_name = "ShadowMapDepth";
        desc.clear_depth = 1.0f;

        return desc;
    }

    void BaseSceneFeature::initialize(SharedRenderService& resources) {
        auto* gfx = resources.getGfxContext();
        auto* deletion_queue = resources.getRenderTargetSystem()
            ? resources.getRenderTargetSystem()->getDeletionQueue()
            : nullptr;
        m_shared_render_service = &resources;
        m_deletion_queue = deletion_queue;

        m_gbuffer = create_scope<RenderTargetHandle>();
        m_gbuffer->initialize(BuildGBufferDesc(), *gfx, deletion_queue);

        m_shadow_map = create_scope<RenderTargetHandle>();
        m_shadow_map->initialize(BuildShadowMapDesc(), *gfx, deletion_queue);
    }

    void BaseSceneFeature::onResize(const UInt32 width, const UInt32 height) {
        auto* gfx = m_shared_render_service ? m_shared_render_service->getGfxContext() : nullptr;
        DO_ASSERT(gfx != nullptr, "BaseSceneFeature onResize requires valid GfxContext");

        if (m_gbuffer) {
            m_gbuffer->resolve(width, height, *gfx, 0);
        }
        if (m_shadow_map) {
            m_shadow_map->resolve(width, height, *gfx, 0);
        }
    }

    void BaseSceneFeature::shutdown() {
        if (m_gbuffer) {
            m_gbuffer->shutdown();
            m_gbuffer.reset();
        }
        if (m_shadow_map) {
            m_shadow_map->shutdown();
            m_shadow_map.reset();
        }
        m_primitive_scene_buffer.reset();
        m_primitive_scene_capacity = 0;
        m_deletion_queue = nullptr;
    }

    void BaseSceneFeature::ensurePrimitiveSceneBufferCapacity(
        const UInt32 instance_count,
        GfxContext& gfx,
        const UInt64 current_frame)
    {
        const UInt32 required = std::max(instance_count, 1u);
        if (required <= m_primitive_scene_capacity && m_primitive_scene_buffer) {
            return;
        }

        const UInt32 new_capacity = std::max(required,
                                              std::max(m_primitive_scene_capacity * 2, 64u));

        GfxBufferDesc desc{};
        desc.byteSize = new_capacity * sizeof(InstanceSceneData);
        desc.format = GfxFormat::Unknown;
        desc.state = GfxResourceStates::VertexBuffer;
        desc.debugName = "BaseScene PrimitiveSceneBuffer";

        auto new_buffer = create_ref<GfxBuffer>(desc, desc.debugName);
        new_buffer->initializeRHI(gfx.getDevice());

        if (m_primitive_scene_buffer && m_deletion_queue) {
            auto old_buffer = m_primitive_scene_buffer;
            m_deletion_queue->enqueueFunc(
                [old_buffer]() mutable { old_buffer.reset(); },
                current_frame + kDeferredFrameDelay);
        }

        m_primitive_scene_buffer = new_buffer;
        m_primitive_scene_capacity = new_capacity;
    }

    void BaseSceneFeature::registerPass(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const {
        GBufferPass{}.build(graph, context);
        DirectionalShadowPass{}.build(graph, context);
        SkyboxPass{}.build(graph, context);
    }

    void LightingFeature::initialize(SharedRenderService& resources) {
        (void)resources;
        m_constant_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(256)
                .setIsConstantBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::ConstantBuffer)
                .setDebugName("DeferredLightPass ConstantBuffer"));
    }

    void LightingFeature::shutdown() {
        m_constant_buffer.reset();
    }

    void LightingFeature::registerPass(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const {
        DeferredLightPass{m_constant_buffer}.build(graph, context);
    }

    void PostProcessFeature::registerPass(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const {
        PostProcessPass{}.build(graph, context);
    }

    void PostProcess2DFeature::registerPass(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const {
        PostProcess2DPass{}.build(graph, context);
    }

    void PresentFeature::registerPass(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const {
        PresentPass{}.build(graph, context);
    }

} // namespace dodoe
