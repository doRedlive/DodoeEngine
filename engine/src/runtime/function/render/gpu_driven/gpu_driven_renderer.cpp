// do@Redlive

#include "gpu_driven_renderer.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    Bool GpuCulling::initialize(const GpuCullingCreateInfo& info) {
        m_gfx = info.gfx_context;
        m_shader_library = info.shader_library;
        m_enabled = RenderSettings::IsGpuDrivenSupported();
        return true;
    }

    void GpuCulling::shutdown() {
        m_bucket_offsets_buffer = nullptr;
        m_bucket_counts_buffer = nullptr;
        m_indirect_args_buffer = nullptr;
        m_visible_count_readback_buffer = nullptr;
        m_culling_params_buffer = nullptr;
        m_visible_count_buffer = nullptr;
        m_visible_objects_buffer = nullptr;
        m_bucket_fill_binding_layout = nullptr;
        m_bucket_fill_pipeline = nullptr;
        m_bucket_count_binding_layout = nullptr;
        m_bucket_count_pipeline = nullptr;
        m_culling_binding_layout = nullptr;
        m_culling_pipeline = nullptr;
        m_shader_library = nullptr;
        m_gfx = nullptr;
        m_enabled = false;
    }

    void GpuCulling::ensureReadbackBuffer(DrawCommandList& cmd_list) {
        if (m_visible_count_readback_buffer) return;
        m_visible_count_readback_buffer = cmd_list.createBuffer(
            GfxBufferDesc()
                .setByteSize(sizeof(UInt32))
                .setCpuAccess(GfxCpuAccessMode::Read)
                .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                .setDebugName("VisibleCountReadback"));
    }

    void GpuCulling::ensureBucketBuffers(DrawCommandList& cmd_list, UInt32 object_count) {
        const UInt32 bucket_buffer_size = kMaxBuckets * static_cast<UInt32>(sizeof(BucketCount));
        if (!m_bucket_counts_buffer) {
            m_bucket_counts_buffer = cmd_list.createBuffer(
                GfxBufferDesc()
                    .setByteSize(bucket_buffer_size)
                    .setStructStride(sizeof(BucketCount))
                    .setCanHaveUAVs(true)
                    .enableAutomaticStateTracking(GfxResourceStates::UnorderedAccess)
                    .setDebugName("BucketCounts"));
        }

        if (!m_bucket_offsets_buffer) {
            m_bucket_offsets_buffer = cmd_list.createBuffer(
                GfxBufferDesc()
                    .setByteSize(bucket_buffer_size)
                    .setStructStride(sizeof(BucketCount))
                    .setCanHaveUAVs(true)
                    .enableAutomaticStateTracking(GfxResourceStates::UnorderedAccess)
                    .setDebugName("BucketOffsets"));
        }

        const UInt64 indirect_args_size = std::max(object_count, 1u) * static_cast<UInt32>(sizeof(DrawIndexedIndirectArgs));
        if (!m_indirect_args_buffer ||
            m_indirect_args_buffer->getByteSize() < indirect_args_size) {
            m_indirect_args_buffer = cmd_list.createBuffer(
                GfxBufferDesc()
                    .setByteSize(indirect_args_size)
                    .setIsDrawIndirectArgs(true)
                    .setCanHaveUAVs(true)
                    .enableAutomaticStateTracking(GfxResourceStates::UnorderedAccess)
                    .setDebugName("IndirectArgs"));
        }
    }

    void GpuCulling::executeCulling(DrawCommandList& cmd_list,
                                           const GpuScenePassResources& scene_resources,
                                           const Matrix4f& view_projection,
                                           UInt32 object_count) {
        if (!m_enabled || !m_shader_library || object_count == 0) return;

        const auto cs = m_shader_library->getGpuCullingComputeShader();
        if (!cs) return;

        const UInt32 effective_count = std::max(object_count, 1u);

        if (!m_culling_binding_layout) {
            GfxBindingLayoutDesc layout_desc;
            layout_desc.addItem(GfxBindingLayoutItem::ConstantBuffer(0));
            layout_desc.addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(0));
            layout_desc.addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(1));
            layout_desc.addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(2));
            layout_desc.addItem(GfxBindingLayoutItem::StructuredBuffer_UAV(0));
            layout_desc.addItem(GfxBindingLayoutItem::StructuredBuffer_UAV(1));
            m_culling_binding_layout = cmd_list.createBindingLayout(layout_desc);
        }

        if (!m_culling_pipeline) {
            GfxComputePipelineDesc pipeline_desc;
            pipeline_desc.setComputeShader(cs);
            pipeline_desc.addBindingLayout(m_culling_binding_layout);
            m_culling_pipeline = m_gfx->getDevice()->createComputePipeline(pipeline_desc);
        }

        if (!m_culling_params_buffer) {
            m_culling_params_buffer = cmd_list.createBuffer(
                GfxBufferDesc()
                    .setByteSize(sizeof(CullingParams))
                    .setIsConstantBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::ConstantBuffer)
                    .setDebugName("CullingParams"));
        }

        const UInt64 visible_buffer_size = effective_count * sizeof(UInt32);
        if (!m_visible_objects_buffer ||
            m_visible_objects_buffer->getByteSize() < visible_buffer_size) {
            m_visible_objects_buffer = cmd_list.createBuffer(
                GfxBufferDesc()
                    .setByteSize(visible_buffer_size)
                    .setStructStride(sizeof(UInt32))
                    .setCanHaveUAVs(true)
                    .enableAutomaticStateTracking(GfxResourceStates::UnorderedAccess)
                    .setDebugName("VisibleObjects"));
        }

        if (!m_visible_count_buffer) {
            m_visible_count_buffer = cmd_list.createBuffer(
                GfxBufferDesc()
                    .setByteSize(sizeof(UInt32))
                    .setCanHaveUAVs(true)
                    .enableAutomaticStateTracking(GfxResourceStates::UnorderedAccess)
                    .setDebugName("VisibleCount"));
        }

        ensureReadbackBuffer(cmd_list);

        CullingParams params{};
        const Matrix4f& m = view_projection;
        params.frustum_planes[0] = Vector4f(m[3] + m[0]);
        params.frustum_planes[1] = Vector4f(m[3] - m[0]);
        params.frustum_planes[2] = Vector4f(m[3] + m[1]);
        params.frustum_planes[3] = Vector4f(m[3] - m[1]);
        params.frustum_planes[4] = Vector4f(m[3] + m[2]);
        params.frustum_planes[5] = Vector4f(m[3] - m[2]);
        params.object_count = effective_count;

        cmd_list.setBufferState(m_culling_params_buffer, GfxResourceStates::CopyDest);
        cmd_list.commitBarriers();
        cmd_list.writeBuffer(m_culling_params_buffer, &params, sizeof(CullingParams), 0);
        cmd_list.setBufferState(m_culling_params_buffer, GfxResourceStates::ConstantBuffer);
        cmd_list.commitBarriers();

        const UInt32 zero = 0;
        cmd_list.setBufferState(m_visible_count_buffer, GfxResourceStates::CopyDest);
        cmd_list.commitBarriers();
        cmd_list.writeBuffer(m_visible_count_buffer, &zero, sizeof(UInt32), 0);
        cmd_list.setBufferState(m_visible_count_buffer, GfxResourceStates::UnorderedAccess);
        cmd_list.commitBarriers();

        GfxBindingSetDesc binding_desc;
        binding_desc.addItem(GfxBindingSetItem::ConstantBuffer(0, m_culling_params_buffer->getRHIHandle()));
        binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(0, scene_resources.object_meta->getRHIHandle()));
        binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(1, scene_resources.transforms->getRHIHandle()));
        binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(2, scene_resources.bounds->getRHIHandle()));
        binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_UAV(0, m_visible_objects_buffer->getRHIHandle()));
        binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_UAV(1, m_visible_count_buffer->getRHIHandle()));

        auto binding_set = cmd_list.createBindingSet(binding_desc, m_culling_binding_layout);

        GfxComputeState compute_state;
        compute_state.setPipeline(m_culling_pipeline);
        compute_state.addBindingSet(binding_set->getRHIHandle());

        cmd_list.setComputeState(compute_state);

        const UInt32 thread_groups = (effective_count + 63) / 64;
        cmd_list.dispatch(thread_groups, 1, 1);

        cmd_list.setBufferState(m_visible_count_buffer, GfxResourceStates::CopySource);
        cmd_list.setBufferState(m_visible_count_readback_buffer, GfxResourceStates::CopyDest);
        cmd_list.commitBarriers();
        cmd_list.copyBuffer(m_visible_count_readback_buffer, 0, m_visible_count_buffer, 0, sizeof(UInt32));

        cmd_list.setBufferState(m_visible_objects_buffer, GfxResourceStates::ShaderResource);
        cmd_list.setBufferState(m_visible_count_buffer, GfxResourceStates::ShaderResource);
        cmd_list.commitBarriers();

        m_frame_index = (m_frame_index + 1) % kMaxFramesInFlight;
        m_object_count = effective_count;
    }

    void GpuCulling::executeBucketBuild(DrawCommandList& cmd_list,
                                         const GpuScenePassResources& scene_resources,
                                         UInt32 object_count) {
        if (!m_enabled || !m_shader_library || object_count == 0) return;
        if (!m_visible_objects_buffer || !m_visible_count_buffer) return;

        ensureBucketBuffers(cmd_list, object_count);

        const auto bucket_count_cs = m_shader_library->getBucketCountComputeShader();
        const auto bucket_fill_cs = m_shader_library->getBucketFillComputeShader();

        if (!m_bucket_count_binding_layout) {
            GfxBindingLayoutDesc layout_desc;
            layout_desc.addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(0));
            layout_desc.addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(1));
            layout_desc.addItem(GfxBindingLayoutItem::ConstantBuffer(1));
            layout_desc.addItem(GfxBindingLayoutItem::StructuredBuffer_UAV(0));
            m_bucket_count_binding_layout = cmd_list.createBindingLayout(layout_desc);
        }

        if (!m_bucket_count_pipeline && bucket_count_cs) {
            GfxComputePipelineDesc pipeline_desc;
            pipeline_desc.setComputeShader(bucket_count_cs);
            pipeline_desc.addBindingLayout(m_bucket_count_binding_layout);
            m_bucket_count_pipeline = m_gfx->getDevice()->createComputePipeline(pipeline_desc);
        }

        if (!m_bucket_fill_binding_layout) {
            GfxBindingLayoutDesc layout_desc;
            layout_desc.addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(0));
            layout_desc.addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(1));
            layout_desc.addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(2));
            layout_desc.addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(3));
            layout_desc.addItem(GfxBindingLayoutItem::StructuredBuffer_UAV(0));
            layout_desc.addItem(GfxBindingLayoutItem::StructuredBuffer_UAV(1));
            m_bucket_fill_binding_layout = cmd_list.createBindingLayout(layout_desc);
        }

        if (!m_bucket_fill_pipeline && bucket_fill_cs) {
            GfxComputePipelineDesc pipeline_desc;
            pipeline_desc.setComputeShader(bucket_fill_cs);
            pipeline_desc.addBindingLayout(m_bucket_fill_binding_layout);
            m_bucket_fill_pipeline = m_gfx->getDevice()->createComputePipeline(pipeline_desc);
        }

        if (bucket_count_cs && m_bucket_count_pipeline) {
            const UInt32 bucket_size = kMaxBuckets * static_cast<UInt32>(sizeof(BucketCount));
            const UInt32 zero_bucket[4] = {0, 0, 0, 0};
            cmd_list.setBufferState(m_bucket_counts_buffer, GfxResourceStates::CopyDest);
            cmd_list.commitBarriers();
            cmd_list.writeBuffer(m_bucket_counts_buffer, zero_bucket, sizeof(zero_bucket), 0);
            cmd_list.setBufferState(m_bucket_counts_buffer, GfxResourceStates::UnorderedAccess);
            cmd_list.commitBarriers();

            const UInt32 count_params[4] = {object_count, kMaxBuckets, 0, 0};
            cmd_list.writeBuffer(m_culling_params_buffer, count_params, sizeof(count_params), 0);

            GfxBindingSetDesc count_binding_desc;
            count_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(0, m_visible_objects_buffer->getRHIHandle()));
            count_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(1, scene_resources.object_meta->getRHIHandle()));
            count_binding_desc.addItem(GfxBindingSetItem::ConstantBuffer(1, m_culling_params_buffer->getRHIHandle()));
            count_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_UAV(0, m_bucket_counts_buffer->getRHIHandle()));

            auto count_binding_set = cmd_list.createBindingSet(count_binding_desc, m_bucket_count_binding_layout);

            GfxComputeState count_state;
            count_state.setPipeline(m_bucket_count_pipeline);
            count_state.addBindingSet(count_binding_set->getRHIHandle());
            cmd_list.setComputeState(count_state);

            const UInt32 count_groups = (object_count + 63) / 64;
            cmd_list.dispatch(count_groups, 1, 1);

            cmd_list.setBufferState(m_bucket_counts_buffer, GfxResourceStates::ShaderResource);
            cmd_list.commitBarriers();
        }

        if (bucket_fill_cs && m_bucket_fill_pipeline) {
            cmd_list.setBufferState(m_indirect_args_buffer, GfxResourceStates::UnorderedAccess);
            cmd_list.commitBarriers();

            GfxBindingSetDesc fill_binding_desc;
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(0, m_visible_objects_buffer->getRHIHandle()));
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(1, scene_resources.object_meta->getRHIHandle()));
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(2, scene_resources.transforms->getRHIHandle()));
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(3, m_bucket_counts_buffer->getRHIHandle()));
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_UAV(0, m_indirect_args_buffer->getRHIHandle()));
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_UAV(1, m_bucket_offsets_buffer->getRHIHandle()));

            auto fill_binding_set = cmd_list.createBindingSet(fill_binding_desc, m_bucket_fill_binding_layout);

            GfxComputeState fill_state;
            fill_state.setPipeline(m_bucket_fill_pipeline);
            fill_state.addBindingSet(fill_binding_set->getRHIHandle());
            cmd_list.setComputeState(fill_state);

            const UInt32 fill_groups = (object_count + 63) / 64;
            cmd_list.dispatch(fill_groups, 1, 1);

            cmd_list.setBufferState(m_indirect_args_buffer, GfxResourceStates::IndirectArgument);
            cmd_list.commitBarriers();
        }
    }

    GpuVisibleStats GpuCulling::getLastVisibleStats() const {
        GpuVisibleStats stats{};
        stats.object_count = m_object_count;
        return stats;
    }

} // dodoe
