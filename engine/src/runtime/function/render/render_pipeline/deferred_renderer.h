// do@Redlive

#pragma once

#include "dopch.h"

#include "renderer.h"
#include "runtime/function/render/framework/local_vertex_factory.h"
#include "runtime/function/render/mesh_draw/mesh_processor_base.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"

namespace dodoe {

    class DeferredRenderer final : public RendererBase {
        Scope<LocalVertexFactory> m_local_vertex_factory{nullptr};
        StaticArray<Scope<IMeshPassProcessor>, static_cast<size_t>(MeshPassType::Count)> m_mesh_processors{};

        [[nodiscard]] RenderPassContext buildPassContext(const RenderScene& scene) const override;

        void initViews(const RenderScene& scene, RenderViewFamily& view_family) const override;

        void setupMeshPassRelevance(RenderView& view) const;
        void setupMeshPassContexts(const RenderScene& scene, RenderViewFamily& view_family) const;
        void buildMeshDrawCommands(RenderViewFamily& view_family, DrawCommandList& cmd_list) const;

    public:
        ~DeferredRenderer() override = default;

        Bool initialize(const RendererCreateInfo& info);
        void shutdown();

        void render(RenderViewFamily& view_family, RenderScene& scene,
                    UInt32 swapchain_image_index, DrawCommandList& out_commands) override;
    };

} // dodoe
