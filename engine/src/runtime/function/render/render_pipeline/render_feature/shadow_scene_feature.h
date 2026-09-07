// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/render/render_service/render_target_handle.h"
#include "runtime/function/render/render_pipeline/passes/render_shadow_pass.h"
#include "runtime/function/render/mesh_draw/mesh_draw_list.h"
#include "runtime/function/render/mesh_draw/cached_mesh_draw_command.h"
#include "runtime/function/render/mesh_draw/mesh_pass_command_storage.h"
#include "runtime/function/render/mesh_draw/mesh_processor_base.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class RenderViewFamily;
    class DrawCommandList;
    class ThreadPool;

    class ShadowSceneFeature final : public IRenderFeature {
        Scope<RenderTargetHandle> m_shadow_map{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};

    public:
        void initialize(SharedRenderService& resources) override;
        void onResize(UInt32 width, UInt32 height) override;
        void shutdown() override;

        void registerGraphImports(RenderGraphImportRegistry& imports,
                                  const RenderView& view) override;

        void collectPasses(PassCollector& collector) override;

        void buildShadowDrawCommands(RenderViewFamily& view_family, DrawCommandList& cmd_list,
                                     ThreadPool* thread_pool = nullptr);

        [[nodiscard]] RenderTargetHandle* getShadowMap() const { return m_shadow_map.get(); }
        [[nodiscard]] MeshPassProcessor* getMeshProcessor() const;
        [[nodiscard]] const MeshDrawCommandCache& getMeshDrawCache() const;
        [[nodiscard]] const DynamicArray<MeshDrawList>& getShadowDrawLists() const;
        [[nodiscard]] const DynamicArray<MeshDrawGpuBucket>& getGpuBuckets(Size_t view_index) const;
    };

} // namespace dodoe
