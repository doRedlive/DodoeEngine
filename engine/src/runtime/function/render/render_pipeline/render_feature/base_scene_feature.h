// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/render/render_service/render_target_handle.h"
#include "runtime/function/render/render_pipeline/passes/render_base_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_skybox_pass.h"
#include "runtime/function/render/mesh_draw/mesh_draw_list.h"
#include "runtime/function/render/mesh_draw/cached_mesh_draw_command.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class GBufferMeshProcessor;
    class DirectionalShadowMeshProcessor;
    class RenderScene;
    class RenderViewFamily;
    class DrawCommandList;

    class BaseSceneFeature final : public IRenderFeature {
        Scope<RenderTargetHandle> m_gbuffer{nullptr};
        Scope<RenderTargetHandle> m_shadow_map{nullptr};
        GfxBufferHandle m_primitive_scene_buffer{};
        UInt32 m_primitive_scene_capacity{0};
        SharedRenderService* m_shared_render_service{nullptr};
        DeferredDeletionQueue* m_deletion_queue{nullptr};
        Scope<GBufferMeshProcessor> m_gbuffer_processor{nullptr};
        Scope<DirectionalShadowMeshProcessor> m_shadow_processor{nullptr};
        GfxBufferHandle m_skybox_cb{};
        MeshDrawCommandCache m_mesh_draw_cache{};
        DynamicArray<MeshDrawList> m_gbuffer_draw_lists;
        DynamicArray<MeshDrawList> m_shadow_draw_lists;

        void setupMeshPassRelevance(RenderView& view) const;
        void buildGpuDrivenDrawCommands(const RenderScene& scene,
                                        RenderViewFamily& view_family,
                                        DrawCommandList& cmd_list) const;

    public:
        void initialize(SharedRenderService& resources) override;
        void onResize(UInt32 width, UInt32 height) override;
        void shutdown() override;

        void exportResources(ResourceRegistry& registry,
                             const RenderView& view) override;

        void collectPasses(PassCollector& collector) override;

        void ensurePrimitiveSceneBufferCapacity(UInt32 instance_count, GfxContext& gfx, UInt64 current_frame);

        void setupMeshPassContexts(const RenderScene& scene, RenderViewFamily& view_family) const;
        void buildMeshDrawCommands(RenderViewFamily& view_family, DrawCommandList& cmd_list);

        [[nodiscard]] RenderTargetHandle* getGBuffer() const { return m_gbuffer.get(); }
        [[nodiscard]] RenderTargetHandle* getShadowMap() const { return m_shadow_map.get(); }
        [[nodiscard]] GfxBufferHandle getPrimitiveSceneBuffer() const { return m_primitive_scene_buffer; }
        [[nodiscard]] UInt32 getPrimitiveSceneCapacity() const { return m_primitive_scene_capacity; }
        [[nodiscard]] GBufferMeshProcessor* getGBufferProcessor() const { return m_gbuffer_processor.get(); }
        [[nodiscard]] DirectionalShadowMeshProcessor* getShadowProcessor() const { return m_shadow_processor.get(); }
        [[nodiscard]] const MeshDrawCommandCache& getMeshDrawCache() const { return m_mesh_draw_cache; }
        [[nodiscard]] const DynamicArray<MeshDrawList>& getGBufferDrawLists() const { return m_gbuffer_draw_lists; }
        [[nodiscard]] const DynamicArray<MeshDrawList>& getShadowDrawLists() const { return m_shadow_draw_lists; }
    };

} // namespace dodoe
