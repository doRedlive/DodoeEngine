// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_scene/render_scene.h"
#include "render_pass_context.h"
#include "../render_view/render_view_family.h"

#include "render_feature/render_feature.h"
#include "runtime/function/render/framework/local_vertex_factory.h"
#include "runtime/function/render/framework/shared_render_service.h"
#include "runtime/core/thread/thread_pool.h"
#include "runtime/function/render/mesh_draw/mesh_processor_base.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    class RenderGraphBuilder;

    struct RenderPipelineCreateInfo {
        Size_t worker_count{0};
        GfxContext* gfx_context{nullptr};
        SharedRenderService* shared_render_service{nullptr};
    };

    class RenderPipeline : public Managed<RenderPipeline, RenderPipelineCreateInfo> {
        friend class Managed<RenderPipeline, RenderPipelineCreateInfo>;

        Scope<ThreadPool> m_thread_pool{nullptr};
        GfxContext* m_gfx_context{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};
        Scope<LocalVertexFactory> m_local_vertex_factory{nullptr};
        StaticArray<Scope<IMeshPassProcessor>, static_cast<size_t>(MeshPassType::Count)> m_mesh_processors{};
        DynamicArray<Scope<IRenderFeature>> m_features{};
        GfxBufferHandle m_deferred_light_constant_buffer{};

    public:
        RenderPipeline() = default;
        ~RenderPipeline() = default;

        void render(RenderViewFamily& view_family, RenderScene& scene, const UInt32 swapchain_image_index, DrawCommandList& out_commands);

    private:
        Bool initialize(const RenderPipelineCreateInfo& info);
        void shutdown();
        [[nodiscard]] RenderPassContext buildPassContext(const RenderScene& scene) const;
        void initViews(const RenderScene& scene, RenderViewFamily& view_family) const;
        void setupMeshPassRelevance(RenderView& view) const;
        void setupMeshPassContexts(const RenderScene& scene, RenderViewFamily& view_family) const;
        void buildMeshDrawCommands(RenderViewFamily& view_family, DrawCommandList& cmd_list) const;
        void buildFrameCommandList(
            const RenderViewFamily& view_family,
            RenderScene& scene,
            const UInt32 swapchain_image_index,
            DrawCommandList& out_commands) const;
        void executeFrameGraph(
            RenderGraphBuilder& graph,
            const RenderViewFamily& view_family,
            RenderScene& scene,
            const RenderView& view,
            const Size_t view_index,
            const UInt32 swapchain_image_index,
            DrawCommandList& out_commands) const;
    };

} // dodoe
