// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/render/render_service/render_target_handle.h"
#include "runtime/function/render/render_pipeline/passes/render_base_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_opaque_pass.h"
#include "runtime/function/render/mesh_draw/mesh_draw_list.h"
#include "runtime/function/render/mesh_draw/cached_mesh_draw_command.h"
#include "runtime/function/render/mesh_draw/lit_mesh_processor.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    enum class OpaqueFillMode : UInt8 {
        GBuffer,
        SceneColor
    };

    class LitMeshProcessor;
    class RenderScene;
    class RenderViewFamily;
    class DrawCommandList;

    class OpaqueSceneFeature final : public IRenderFeature {
        OpaqueFillMode m_fill_mode{OpaqueFillMode::GBuffer};
        Scope<RenderTargetHandle> m_gbuffer{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};
        Scope<LitMeshProcessor> m_lit_processor{nullptr};
        MeshDrawCommandCache m_mesh_draw_cache{};
        DynamicArray<MeshDrawList> m_lit_draw_lists;

    public:
        explicit OpaqueSceneFeature(const OpaqueFillMode fill_mode)
            : m_fill_mode(fill_mode) {}

        void initialize(SharedRenderService& resources) override;
        void onResize(UInt32 width, UInt32 height) override;
        void shutdown() override;

        void registerGraphImports(RenderGraphImportRegistry& imports,
                                  const RenderView& view) override;

        void collectPasses(PassCollector& collector) override;

        void setupMeshPassContexts(const RenderScene& scene, RenderViewFamily& view_family) const;
        void buildMeshDrawCommands(RenderViewFamily& view_family, DrawCommandList& cmd_list);

        [[nodiscard]] OpaqueFillMode getFillMode() const { return m_fill_mode; }
        [[nodiscard]] RenderTargetHandle* getGBuffer() const { return m_gbuffer.get(); }
        [[nodiscard]] LitMeshProcessor* getLitProcessor() const { return m_lit_processor.get(); }
        [[nodiscard]] const MeshDrawCommandCache& getMeshDrawCache() const { return m_mesh_draw_cache; }
        [[nodiscard]] const DynamicArray<MeshDrawList>& getLitDrawLists() const { return m_lit_draw_lists; }
    };

} // namespace dodoe
