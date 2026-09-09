// do@Redlive

#pragma once

#include "dopch.h"

#include "gpu_scene.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    struct GpuCullingCreateInfo {
        GfxContext* gfx_context{nullptr};
        ShaderLibrary* shader_library{nullptr};
    };

    struct GpuBucketDrawSnapshot {
        GfxGraphicsPipelineHandle pipeline{};
        DynamicArray<GfxBindingSetHandle> binding_sets{};
        DynamicArray<GfxVertexBufferBinding> vertex_bindings{};
        GfxIndexBufferBinding index_binding{};
        PrimitiveMeshDrawShaderData shader_data{};
    };

    struct GpuVisibleStats {
        UInt32 visible_count{0};
        UInt32 object_count{0};
        UInt32 bucket_count{0};
        UInt32 indirect_args_count{0};
    };

    struct GpuBucketCpuDraw {
        UInt32 template_index{0};
        UInt32 first_arg{0};
        UInt32 arg_count{0};
    };

    class GpuCulling : public Managed<GpuCulling, GpuCullingCreateInfo> {
        friend class Managed<GpuCulling, GpuCullingCreateInfo>;

    public:
        static constexpr UInt32 kMaxBuckets = 4096;
        static constexpr UInt32 kMaxGenerations = 3;

        void setEnabled(Bool enabled) { m_enabled = enabled; }
        [[nodiscard]] Bool isEnabled() const { return m_enabled; }

        void executeCulling(DrawCommandList& cmd_list,
                           const GpuScenePassResources& scene_resources,
                           const Matrix4f& view_projection,
                           UInt32 object_count);

        void executeBucketBuild(DrawCommandList& cmd_list,
                               const GpuScenePassResources& scene_resources,
                               UInt32 object_count);

        [[nodiscard]] GfxBufferHandle getVisibleObjectsBuffer() const { return m_visible_objects_buffer; }
        [[nodiscard]] GfxBufferHandle getVisibleCountBuffer() const { return m_visible_count_buffer; }
        [[nodiscard]] GfxBufferHandle getIndirectArgsBuffer(UInt32 generation_back = 0) const {
            const UInt32 generation = m_generation_counter - 1 - generation_back;
            return generation_back < m_generation_counter
                ? m_indirect_args_buffer[generation % kMaxGenerations] : GfxBufferHandle{};
        }
        [[nodiscard]] GfxBufferHandle getBucketRangesBuffer() const { return m_bucket_ranges_buffer; }

        void uploadBucketTemplates(DrawCommandList& cmd_list,
                                   const GpuBucketTemplateUpload* uploads,
                                   const UInt32 upload_count,
                                   const GpuBucketDrawSnapshot* snapshots,
                                   const UInt32 snapshot_count);

        Bool acquireBucketDraws(DynamicArray<GpuBucketCpuDraw>& out_draws);

        void debugLogGpuBuckets(DynamicArray<GpuBucketCpuDraw>& draws);

        [[nodiscard]] const DynamicArray<GpuBucketDrawSnapshot>& getBucketDrawSnapshots(const UInt32 generation_back) const {
            static const DynamicArray<GpuBucketDrawSnapshot> empty{};
            if (generation_back >= m_generation_counter) {
                return empty;
            }
            const UInt32 generation = m_generation_counter - 1 - generation_back;
            return m_bucket_draw_snapshots[generation % kMaxGenerations];
        }

        [[nodiscard]] GpuVisibleStats getLastVisibleStats() const;

    private:
        GfxContext* m_gfx{nullptr};
        ShaderLibrary* m_shader_library{nullptr};

        GfxComputePipelineHandle m_culling_pipeline{};
        GfxBindingLayoutHandle m_culling_binding_layout{};

        GfxComputePipelineHandle m_bucket_count_pipeline{};
        GfxBindingLayoutHandle m_bucket_count_binding_layout{};

        GfxComputePipelineHandle m_bucket_scan_pipeline{};
        GfxBindingLayoutHandle m_bucket_scan_binding_layout{};

        GfxComputePipelineHandle m_bucket_fill_pipeline{};
        GfxBindingLayoutHandle m_bucket_fill_binding_layout{};

        GfxBufferHandle m_visible_objects_buffer{};
        GfxBufferHandle m_visible_count_buffer{};
        GfxBufferHandle m_visible_count_readback_buffer{};
        GfxBufferHandle m_culling_params_buffer{};
        GfxBufferHandle m_indirect_args_buffer[kMaxGenerations]{};
        GfxBufferHandle m_bucket_counts_buffer{};
        GfxBufferHandle m_bucket_counts_readback_buffer{};
        GfxBufferHandle m_bucket_templates_buffer[kMaxGenerations]{};
        GfxBufferHandle m_bucket_template_map_buffer[kMaxGenerations]{};
        GfxBufferHandle m_bucket_bases_buffer{};
        GfxBufferHandle m_bucket_ranges_buffer{};
        GfxBufferHandle m_bucket_ranges_readback_buffer[kMaxGenerations]{};
        DynamicArray<UInt32> m_template_map_history[kMaxGenerations]{};
        DynamicArray<GpuBucketDrawSnapshot> m_bucket_draw_snapshots[kMaxGenerations]{};
        UInt32 m_template_count_history[kMaxGenerations]{};
        DynamicArray<GfxBindingSetHandle> m_frame_binding_sets{};
        UInt32 m_binding_sets_generation{0};

        UInt32 m_object_count{4096};
        UInt32 m_template_count{0};
        UInt32 m_generation_counter{0};
        Bool m_enabled{false};

        Bool initialize(const GpuCullingCreateInfo& info);
        void shutdown();

        void ensureReadbackBuffer(DrawCommandList& cmd_list);
        void ensureBucketBuffers(DrawCommandList& cmd_list, UInt32 object_count);
    };

} // dodoe
