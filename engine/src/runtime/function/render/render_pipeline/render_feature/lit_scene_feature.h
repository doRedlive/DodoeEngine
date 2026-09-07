// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_feature/render_feature.h"
#include "runtime/function/render/mesh_draw/mesh_draw_list.h"
#include "runtime/function/render/mesh_draw/cached_mesh_draw_command.h"
#include "runtime/function/render/mesh_draw/mesh_pass_command_storage.h"
#include "runtime/function/render/mesh_draw/mesh_processor_base.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class ShaderLibrary;
    class RenderScene;
    class RenderViewFamily;
    class DrawCommandList;
    class ThreadPool;

    class LitSceneFeature : public IRenderFeature {
        SharedRenderService* m_shared_render_service{nullptr};

    public:
        void initialize(SharedRenderService& resources) override;
        void shutdown() override;

        void setupMeshPassContexts(const RenderScene& scene, RenderViewFamily& view_family) const;
        void buildMeshDrawCommands(RenderViewFamily& view_family, DrawCommandList& cmd_list,
                                   ThreadPool* thread_pool = nullptr);

        [[nodiscard]] MeshPassProcessor* getMeshProcessor() const;
        [[nodiscard]] const MeshDrawCommandCache& getMeshDrawCache() const;
        [[nodiscard]] const DynamicArray<MeshDrawList>& getLitDrawLists() const;
        [[nodiscard]] const DynamicArray<MeshDrawGpuBucket>& getGpuBuckets(Size_t view_index) const;

    protected:
        [[nodiscard]] SharedRenderService* getSharedRenderService() const { return m_shared_render_service; }

        [[nodiscard]] virtual MeshPassType getMeshPassType() const { return MeshPassType::Opaque; }
        [[nodiscard]] virtual GfxShaderHandle getPixelShader(const ShaderLibrary& shader_library) const = 0;
        [[nodiscard]] virtual GfxFramebufferInfo getFramebufferInfo() const = 0;
        [[nodiscard]] virtual bool usesPassBindingLayout() const { return false; }
        [[nodiscard]] MeshPassCommandStorage* getCommandStorage() const;
    };

} // namespace dodoe
