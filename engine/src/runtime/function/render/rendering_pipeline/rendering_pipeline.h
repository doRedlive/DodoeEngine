// do@Redlive

#pragma once

#include "dopch.h"

#include "fullscreen_pass_shared_state.h"
#include "../render_scene/render_scene.h"
#include "rendering_pass_context.h"
#include "render_view_family.h"

#include "runtime/function/render/framework/local_vertex_factory.h"
#include "runtime/function/render/framework/pipeline_state_cache.h"
#include "runtime/function/render/framework/primitive_scene_info.h"
#include "runtime/function/render/framework/shader_library.h"
#include "runtime/core/thread/thread_pool.h"
#include "runtime/function/render/framework/descriptor_table_manager.h"
#include "runtime/function/render/framework/texture_manager.h"
#include "runtime/function/render/mesh_draw/directional_shadow_mesh_processor.h"
#include "runtime/function/render/mesh_draw/gbuffer_mesh_processor.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    class RenderGraphBuilder;

    struct RenderingPipelineCreateInfo {
        Size_t worker_count{0};
        GfxContext* gfx_context{nullptr};
        DescriptorTableManager* descriptor_table{nullptr};
        TextureManager* texture_manager{nullptr};
    };

    class RenderingPipeline : public Managed<RenderingPipeline, RenderingPipelineCreateInfo> {
        friend class Managed<RenderingPipeline, RenderingPipelineCreateInfo>;

        Scope<ThreadPool> m_thread_pool{nullptr};
        GfxContext* m_gfx_context{nullptr};
        DescriptorTableManager* m_descriptor_table{nullptr};
        TextureManager* m_texture_manager{nullptr};
        Scope<ShaderLibrary> m_shader_library{nullptr};
        Scope<LocalVertexFactory> m_local_vertex_factory{nullptr};
        Scope<GBufferMeshProcessor> m_gbuffer_mesh_processor{nullptr};
        Scope<DirectionalShadowMeshProcessor> m_directional_shadow_mesh_processor{nullptr};
        Scope<PipelineStateCache> m_pipeline_state_cache{nullptr};
        Scope<FullscreenPassSharedState> m_fullscreen_pass_shared_state{nullptr};

    public:
        RenderingPipeline() = default;
        ~RenderingPipeline() = default;

        DrawCommandList render(RenderViewFamily& view_family, RenderScene& scene, const UInt32 swapchain_image_index);

    private:
        Bool initialize(const RenderingPipelineCreateInfo& info);
        void shutdown();
        [[nodiscard]] RenderingPassContext buildPassContext() const;
        void initViews(const RenderScene& scene, RenderViewFamily& view_family) const;
        void setupMeshPassRelevance(RenderView& view) const;
        void setupMeshPassContexts(const RenderScene& scene, RenderViewFamily& view_family) const;
        void buildMeshDrawCommands(RenderViewFamily& view_family) const;
        DrawCommandList buildFrameCommandList(
            const RenderViewFamily& view_family,
            RenderScene& scene,
            const UInt32 swapchain_image_index) const;
        DrawCommandList executeFrameGraph(
            RenderGraphBuilder& graph,
            const RenderViewFamily& view_family,
            RenderScene& scene,
            const RenderView& view,
            const Size_t view_index,
            const UInt32 swapchain_image_index) const;
    };

} // dodoe
