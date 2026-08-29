// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/render/render_service/render_target_handle.h"
#include "runtime/function/render/render_pipeline/passes/render_shadow_pass.h"
#include "runtime/function/render/mesh_draw/mesh_draw_list.h"
#include "runtime/function/render/mesh_draw/cached_mesh_draw_command.h"
#include "runtime/function/render/mesh_draw/shadow_mesh_processor.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class ShadowMeshProcessor;
    class RenderViewFamily;
    class DrawCommandList;

    class ShadowSceneFeature final : public IRenderFeature {
        Scope<RenderTargetHandle> m_shadow_map{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};
        Scope<ShadowMeshProcessor> m_shadow_processor{nullptr};
        MeshDrawCommandCache m_mesh_draw_cache{};
        DynamicArray<MeshDrawList> m_shadow_draw_lists;

    public:
        void initialize(SharedRenderService& resources) override;
        void onResize(UInt32 width, UInt32 height) override;
        void shutdown() override;

        void registerGraphImports(RenderGraphImportRegistry& imports,
                                  const RenderView& view) override;

        void collectPasses(PassCollector& collector) override;

        void buildShadowDrawCommands(RenderViewFamily& view_family, DrawCommandList& cmd_list);

        [[nodiscard]] RenderTargetHandle* getShadowMap() const { return m_shadow_map.get(); }
        [[nodiscard]] ShadowMeshProcessor* getShadowProcessor() const { return m_shadow_processor.get(); }
        [[nodiscard]] const MeshDrawCommandCache& getMeshDrawCache() const { return m_mesh_draw_cache; }
        [[nodiscard]] const DynamicArray<MeshDrawList>& getShadowDrawLists() const { return m_shadow_draw_lists; }
    };

} // namespace dodoe
