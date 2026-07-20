// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/gpu_driven/gpu_scene.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    struct GpuCullingCreateInfo {
        GfxContext* gfx_context{nullptr};
        ShaderLibrary* shader_library{nullptr};
    };

    struct GpuVisibleStats {
        UInt32 visible_count{0};
        UInt32 object_count{0};
        UInt32 bucket_count{0};
        UInt32 indirect_args_count{0};
    };

    class GpuCulling : public Managed<GpuCulling, GpuCullingCreateInfo> {
        friend class Managed<GpuCulling, GpuCullingCreateInfo>;

        static constexpr UInt32 kMaxBuckets = 4096;
        static constexpr UInt32 kMaxFramesInFlight = 2;

        GfxContext* m_gfx{nullptr};
        ShaderLibrary* m_shader_library{nullptr};

        GfxComputePipelineHandle m_culling_pipeline{};
        GfxBindingLayoutHandle m_culling_binding_layout{};

        GfxComputePipelineHandle m_bucket_count_pipeline{};
        GfxBindingLayoutHandle m_bucket_count_binding_layout{};

        GfxComputePipelineHandle m_bucket_fill_pipeline{};
        GfxBindingLayoutHandle m_bucket_fill_binding_layout{};

        GfxBufferHandle m_visible_objects_buffer{};
        GfxBufferHandle m_visible_count_buffer{};
        GfxBufferHandle m_visible_count_readback_buffer{};
        GfxBufferHandle m_culling_params_buffer{};
        GfxBufferHandle m_indirect_args_buffer{};
        GfxBufferHandle m_bucket_counts_buffer{};
        GfxBufferHandle m_bucket_offsets_buffer{};

        UInt32 m_frame_index{0};
        UInt32 m_object_count{4096};
        Bool m_enabled{false};

    public:
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
        [[nodiscard]] GfxBufferHandle getIndirectArgsBuffer() const { return m_indirect_args_buffer; }

        [[nodiscard]] GpuVisibleStats getLastVisibleStats() const;

    private:
        Bool initialize(const GpuCullingCreateInfo& info);
        void shutdown();

        void ensureReadbackBuffer(DrawCommandList& cmd_list);
        void ensureBucketBuffers(DrawCommandList& cmd_list, UInt32 object_count);
    };

} // dodoe
