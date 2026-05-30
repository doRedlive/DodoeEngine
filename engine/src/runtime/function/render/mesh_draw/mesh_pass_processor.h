// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_batch.h"
#include "mesh_draw_command.h"
#include "mesh_pipeline_state.h"
#include "../interface/rhi.h"
#include "../interface/rhi_context.h"

namespace dodoe {

    class MeshInstance;

    class MeshPassProcessor {
        inline static UnorderedMap<rhi::ShaderHandle, rhi::InputLayoutHandle> s_input_layout_cache;

        RhiContext* m_rhi{nullptr};
        Scope<MeshPipelineState> m_pipeline_state;
        UnorderedMap<MeshDrawCommandCacheKey, MeshDrawCommand> m_command_cache;

    public:
        MeshPassProcessor() = default;
        ~MeshPassProcessor() = default;

        static void Setup() {};
        static void Cleanup() { s_input_layout_cache.clear(); }

        Bool initialize(RhiContext* rhi, const MeshPipelineStateDesc& desc);
        void shutdown();

        DynamicArray<MeshBatch> buildMeshBatches(
            const DynamicArray<Ref<MeshInstance>>& visible_instances) const;

        using PerBatchConstantsFn = std::function<void(
            rhi::CommandListHandle cmd_list,
            const MeshBatch& batch,
            rhi::BufferHandle constant_buffer)>;

        DynamicArray<MeshDrawCommand> buildDrawCommands(
            const DynamicArray<MeshBatch>& batches,
            rhi::CommandListHandle cmd_list,
            const PerBatchConstantsFn& per_batch_fn = {});

        void submitDrawCommands(
            const DynamicArray<MeshDrawCommand>& commands,
            rhi::FramebufferHandle framebuffer,
            const Vector2i& viewport_extent,
            rhi::CommandListHandle cmd_list) const;

        void createGraphicsPipeline(rhi::FramebufferHandle framebuffer);
        void invalidatePipeline();
        void invalidateCache();

        [[nodiscard]] MeshPipelineState* getPipelineState() { return m_pipeline_state.get(); }
        [[nodiscard]] const MeshPipelineState* getPipelineState() const { return m_pipeline_state.get(); }
        [[nodiscard]] rhi::BufferHandle getConstantBuffer() const;
        [[nodiscard]] Size_t getCacheSize() const { return m_command_cache.size(); }

    private:
        rhi::InputLayoutHandle createStandardInputLayout(
            rhi::ShaderHandle vertex_shader,
            const MeshPipelineStateDesc& desc);
    };

} // dodoe
