// do@Redlive

#pragma once

#include "dopch.h"

#include "srp_types.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"
#include "runtime/function/script/script_command.h"

namespace dodoe {

    class RenderGraphBuilder;
    class RenderGraphPassContext;
    class DrawCommandList;
    class RenderView;
    class BaseRenderer;
    class SharedRenderService;
    class RenderTargetHandle;
    class GfxContext;
    struct RenderPassBuildContext;

    class SrpBridge {
        ScriptCallFn m_call{nullptr};
        BaseRenderer* m_renderer{nullptr};
        SharedRenderService* m_services{nullptr};
        Bool m_pipeline_created{false};
        Bool m_managed_installed{false};
        Int32 m_pipeline_id{0};
        DynamicArray<Int32> m_feature_ids{};
        DynamicArray<Int32> m_feature_phases{};
        DynamicArray<String> m_render_target_names{};

        RenderGraphBuilder* m_current_graph{nullptr};
        const RenderPassBuildContext* m_build_context{nullptr};
        RenderGraphPassContext* m_pass_context{nullptr};
        DrawCommandList* m_command_list{nullptr};

        SrpBridge() = default;

    public:
        static SrpBridge& Self();

        void setScriptCall(ScriptCallFn call);
        void attachRenderer(BaseRenderer* renderer);
        void detachRenderer(BaseRenderer* renderer);

        Bool ensurePipeline(BaseRenderer* renderer);
        void shutdownPipeline();
        void beginFrame();

        [[nodiscard]] Bool isReady() const { return m_call != nullptr; }
        [[nodiscard]] Int32 getFeatureCount() const { return static_cast<Int32>(m_feature_ids.size()); }
        [[nodiscard]] Bool getFeature(Int32 index, Int32& out_id, Int32& out_phase) const;

        void featureInitialize(Int32 feature_id);
        void featureOnResize(Int32 feature_id, UInt32 width, UInt32 height);
        void featureDispose(Int32 feature_id);
        void featureAddPasses(Int32 feature_id, RenderGraphBuilder& graph, const RenderPassBuildContext& context);

        void executePass(Int32 execute_id, RenderGraphPassContext& context, DrawCommandList& command_list);
        void drawRenderers(const SrpMeshDrawSettings& settings);

        Int32 createRenderTarget(const String& name, SrpFormat format, Float scale_x, Float scale_y);
        Int32 findRenderTarget(const String& name);
        void resizeRenderTarget(Int32 id, UInt32 width, UInt32 height);
        [[nodiscard]] RenderTargetHandle* getRenderTarget(Int32 id) const;
        [[nodiscard]] Bool isRenderTargetValid(Int32 id) const;

        UInt32 createGraphTexture(const String& name, UInt32 width, UInt32 height, SrpFormat format, Bool depth, UInt32 sample_count);
        UInt32 importGraphTexture(Int32 render_target, const String& name, Bool depth);
        UInt32 importGraphBackBuffer(const String& name);
        void addRasterPass(const SrpRasterPassDesc& desc);
    };

} // dodoe
