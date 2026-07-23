// do@Redlive

#pragma once

#include "dopch.h"

#include "render_pass_context.h"
#include "render_feature/render_feature.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_view/render_view_family.h"
#include "runtime/function/render/shared_render_service.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/core/thread/thread_pool.h"

namespace dodoe {

    struct RendererCreateInfo {
        Size_t worker_count{0};
        GfxContext* gfx_context{nullptr};
        SharedRenderService* shared_render_service{nullptr};
    };

    class BaseRenderer {
    protected:
        GfxContext*           m_gfx_context{nullptr};
        SharedRenderService*  m_shared_render_service{nullptr};
        Scope<ThreadPool>     m_thread_pool{nullptr};
        DynamicArray<Scope<IRenderFeature>> m_features{};

        [[nodiscard]] RenderPassContext makePassContext(const RenderScene& scene) const;

        void clearViewExtensions(RenderViewFamily& view_family) const;

        void setupFramePasses(RenderViewFamily& view_family, RenderScene& scene,
                              UInt32 swapchain_image_index, DrawCommandList& out_commands) const;

    public:
        virtual ~BaseRenderer() = default;

        template <typename T, typename... Args>
        void addFeature(Args&&... args) {
            m_features.push_back(create_scope<T>(std::forward<Args>(args)...));
        }

        void clearFeatures() { m_features.clear(); }

        void onResize(UInt32 width, UInt32 height) {
            for (const auto& feature : m_features) {
                feature->onResize(width, height);
            }
        }

        [[nodiscard]] GfxContext*          getGfx()           const { return m_gfx_context; }
        [[nodiscard]] SharedRenderService* getSharedService() const { return m_shared_render_service; }
        [[nodiscard]] ThreadPool*          getThreadPool()    const { return m_thread_pool.get(); }

    protected:
        virtual void render(RenderViewFamily& view_family, RenderScene& scene,
                            UInt32 swapchain_image_index, DrawCommandList& out_commands) = 0;
    };

} // dodoe
