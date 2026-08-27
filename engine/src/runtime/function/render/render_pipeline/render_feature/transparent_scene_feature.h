#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/render/render_pipeline/passes/render_opaque_pass.h"
#include "runtime/function/render/mesh_draw/mesh_draw_list.h"
#include "runtime/function/render/mesh_draw/cached_mesh_draw_command.h"
#include "runtime/function/render/mesh_draw/lit_mesh_processor.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class LitMeshProcessor;
    class RenderViewFamily;
    class DrawCommandList;

    class TransparentSceneFeature final : public IRenderFeature {
        SharedRenderService* m_shared_render_service{nullptr};
        Scope<LitMeshProcessor> m_lit_processor{nullptr};
        MeshDrawCommandCache m_mesh_draw_cache{};
        DynamicArray<MeshDrawList> m_transparent_draw_lists;

    public:
        void initialize(SharedRenderService& resources) override;
        void shutdown() override;

        void registerGraphImports(RenderGraphImportRegistry& imports,
                                  const RenderView& view) override;

        void collectPasses(PassCollector& collector) override;

        void buildTransparentDrawCommands(RenderViewFamily& view_family, DrawCommandList& cmd_list);

        [[nodiscard]] LitMeshProcessor* getLitProcessor() const { return m_lit_processor.get(); }
        [[nodiscard]] const MeshDrawCommandCache& getMeshDrawCache() const { return m_mesh_draw_cache; }
        [[nodiscard]] const DynamicArray<MeshDrawList>& getTransparentDrawLists() const { return m_transparent_draw_lists; }
    };

} // namespace dodoe
