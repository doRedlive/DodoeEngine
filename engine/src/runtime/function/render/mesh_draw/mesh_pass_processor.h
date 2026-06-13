// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_batch.h"
#include "mesh_draw_command.h"
#include "mesh_pipeline_state.h"
#include "runtime/function/graphics/gfx.h"
#include "../interface/gfx_context.h"

namespace dodoe {

    class PrimitiveSceneInfo;

    class MeshPassProcessor {
        inline static UnorderedMap<gfx::ShaderHandle, gfx::InputLayoutHandle> s_input_layout_cache;

        GfxContext* m_gfx{nullptr};
        Scope<MeshPipelineState> m_pipeline_state;
        UnorderedMap<MeshDrawCommandCacheKey, MeshDrawCommand> m_command_cache;

    public:
        MeshPassProcessor() = default;
        ~MeshPassProcessor() = default;

        static void Setup() {};
        static void Cleanup() { s_input_layout_cache.clear(); }

        Bool initialize(GfxContext* gfx, const MeshPipelineStateDesc& desc);
        void shutdown();

        DynamicArray<MeshBatch> buildMeshBatches(
            const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
            MeshPassType pass_type) const;

        using PerBatchConstantsFn = std::function<void(
            gfx::CommandListHandle cmd_list,
            const MeshBatch& batch,
            gfx::BufferHandle constant_buffer)>;

        DynamicArray<MeshDrawCommand> buildDrawCommands(
            const DynamicArray<MeshBatch>& batches,
            gfx::CommandListHandle cmd_list,
            const PerBatchConstantsFn& per_batch_fn = {});

        void submitDrawCommands(
            const DynamicArray<MeshDrawCommand>& commands,
            gfx::FramebufferHandle framebuffer,
            const Vector2i& viewport_extent,
            gfx::CommandListHandle cmd_list) const;

        void createGraphicsPipeline(gfx::FramebufferHandle framebuffer);
        void invalidatePipeline();
        void invalidateCache();

        [[nodiscard]] MeshPipelineState* getPipelineState() { return m_pipeline_state.get(); }
        [[nodiscard]] const MeshPipelineState* getPipelineState() const { return m_pipeline_state.get(); }
        [[nodiscard]] gfx::BufferHandle getConstantBuffer() const;
        [[nodiscard]] Size_t getCacheSize() const { return m_command_cache.size(); }

    private:
        gfx::InputLayoutHandle createStandardInputLayout(
            gfx::ShaderHandle vertex_shader,
            const MeshPipelineStateDesc& desc);
    };

} // dodoe
