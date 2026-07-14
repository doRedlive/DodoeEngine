// do@Redlive

#pragma once

#include "dopch.h"

#include "render_pass_context.h"
#include "render_feature/render_feature.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_view/render_view_family.h"
#include "runtime/function/render/framework/shared_render_service.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/core/thread/thread_pool.h"

namespace dodoe {

    class RenderGraphBuilder;

    struct RendererCreateInfo {
        Size_t worker_count{0};
        GfxContext* gfx_context{nullptr};
        SharedRenderService* shared_render_service{nullptr};
    };

    class IRenderer {
    public:
        virtual ~IRenderer() = default;
        virtual void render(RenderViewFamily& view_family, RenderScene& scene,
                            UInt32 swapchain_image_index, DrawCommandList& out_commands) = 0;
    };

    class RendererBase : public IRenderer {
    protected:
        GfxContext* m_gfx_context{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};
        Scope<ThreadPool> m_thread_pool{nullptr};
        DynamicArray<Scope<IRenderFeature>> m_features{};

        [[nodiscard]] virtual RenderPassContext buildPassContext(const RenderScene& scene) const;

        virtual void initViews(const RenderScene& scene, RenderViewFamily& view_family) const;

        void buildFrameDrawCommandList(
            const RenderViewFamily& view_family,
            RenderScene& scene,
            const UInt32 swapchain_image_index,
            DrawCommandList& out_commands) const;

        void executeFrameGraph(
            RenderGraphBuilder& graph,
            const RenderViewFamily& view_family,
            RenderScene& scene,
            const RenderView& view,
            Size_t view_index,
            UInt32 swapchain_image_index,
            DrawCommandList& out_commands) const;

    public:
        [[nodiscard]] GfxContext* getGfxContext() const { return m_gfx_context; }
        [[nodiscard]] SharedRenderService* getSharedRenderService() const { return m_shared_render_service; }

        Bool initializeBase(const RendererCreateInfo& info);
        void shutdownBase();
    };

} // dodoe
