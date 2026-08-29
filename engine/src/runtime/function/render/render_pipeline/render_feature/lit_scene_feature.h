// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_feature/render_feature.h"
#include "runtime/function/render/mesh_draw/mesh_draw_list.h"
#include "runtime/function/render/mesh_draw/cached_mesh_draw_command.h"
#include "runtime/function/render/mesh_draw/lit_mesh_processor.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class ShaderLibrary;
    class RenderScene;
    class RenderViewFamily;
    class DrawCommandList;

    class LitSceneFeature : public IRenderFeature {
        SharedRenderService* m_shared_render_service{nullptr};
        Scope<LitMeshProcessor> m_lit_processor{nullptr};
        MeshDrawCommandCache m_mesh_draw_cache{};
        DynamicArray<MeshDrawList> m_draw_lists;

    public:
        void initialize(SharedRenderService& resources) override;
        void shutdown() override;

        void setupMeshPassContexts(const RenderScene& scene, RenderViewFamily& view_family) const;
        void buildMeshDrawCommands(RenderViewFamily& view_family, DrawCommandList& cmd_list);

        [[nodiscard]] LitMeshProcessor* getLitProcessor() const { return m_lit_processor.get(); }
        [[nodiscard]] const MeshDrawCommandCache& getMeshDrawCache() const { return m_mesh_draw_cache; }
        [[nodiscard]] const DynamicArray<MeshDrawList>& getLitDrawLists() const { return m_draw_lists; }

    protected:
        [[nodiscard]] SharedRenderService* getSharedRenderService() const { return m_shared_render_service; }

        [[nodiscard]] virtual MeshPassType getMeshPassType() const { return MeshPassType::Opaque; }
        [[nodiscard]] virtual GfxShaderHandle getPixelShader(const ShaderLibrary& shader_library) const = 0;
        [[nodiscard]] virtual GfxFramebufferInfo getFramebufferInfo() const = 0;
        [[nodiscard]] virtual bool usesPassBindingLayout() const { return false; }
        virtual void modifyPipelineDesc(GfxGraphicsPipelineDesc& pipeline_desc) const { (void)pipeline_desc; }
    };

} // namespace dodoe
