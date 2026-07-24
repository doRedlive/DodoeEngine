// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/render/render_service/render_target_handle.h"
#include "runtime/function/render/render_pipeline/passes/render_base_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_skybox_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_deferred_light_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_post_process_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_post_process_2d_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_present_pass.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class BaseSceneFeature final : public IRenderFeature {
        Scope<RenderTargetHandle> m_gbuffer{nullptr};
        Scope<RenderTargetHandle> m_shadow_map{nullptr};
        GfxBufferHandle m_primitive_scene_buffer{};
        UInt32 m_primitive_scene_capacity{0};
        SharedRenderService* m_shared_render_service{nullptr};
        DeferredDeletionQueue* m_deletion_queue{nullptr};
        GBufferPass m_gbuffer_pass{};
        DirectionalShadowPass m_shadow_pass{};
        SkyboxPass m_skybox_pass{};

    public:
        void initialize(SharedRenderService& resources) override;
        void onResize(UInt32 width, UInt32 height) override;
        void shutdown() override;

        void setupPasses(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const override;

        void ensurePrimitiveSceneBufferCapacity(UInt32 instance_count, GfxContext& gfx, UInt64 current_frame);

        [[nodiscard]] RenderTargetHandle* getGBuffer() const { return m_gbuffer.get(); }
        [[nodiscard]] RenderTargetHandle* getShadowMap() const { return m_shadow_map.get(); }
        [[nodiscard]] GfxBufferHandle getPrimitiveSceneBuffer() const { return m_primitive_scene_buffer; }
        [[nodiscard]] UInt32 getPrimitiveSceneCapacity() const { return m_primitive_scene_capacity; }
    };

    class LightingFeature final : public IRenderFeature {
    public:
        struct Resource {
            GfxBufferHandle constant_buffer{};
        };

    private:
        Resource m_resource{};

    public:
        void initialize(SharedRenderService& resources) override;
        void shutdown() override;

        void setupPasses(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const override;

        [[nodiscard]] const Resource& resource() const { return m_resource; }
    };

    class PostProcessFeature final : public IRenderFeature {
        PostProcessPass m_post_process_pass{};

    public:
        void setupPasses(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const override;
    };

    class PostProcess2DFeature final : public IRenderFeature {
        PostProcess2DPass m_post_process_2d_pass{};

    public:
        void setupPasses(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const override;
    };

    class PresentFeature final : public IRenderFeature {
        PresentPass m_present_pass{};

    public:
        void setupPasses(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const override;
    };

} // dodoe
