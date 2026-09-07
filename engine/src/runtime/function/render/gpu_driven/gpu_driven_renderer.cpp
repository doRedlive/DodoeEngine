// do@Redlive

#include "gpu_driven_renderer.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/shader/shader_parameter.h"

namespace dodoe {

    Bool GpuCulling::initialize(const GpuCullingCreateInfo& info) {
        m_gfx = info.gfx_context;
        m_shader_library = info.shader_library;
        m_enabled = RenderSettings::IsGpuDrivenSupported();
        return true;
    }

    void GpuCulling::shutdown() {
        for (auto& args : m_indirect_args_buffer) args = nullptr;
        for (auto& templates : m_bucket_templates_buffer) templates = nullptr;
        for (auto& map_buffer : m_bucket_template_map_buffer) map_buffer = nullptr;
        for (auto& readback : m_bucket_ranges_readback_buffer) readback = nullptr;
        for (auto& map_history : m_template_map_history) map_history.clear();
        for (auto& count_history : m_template_count_history) count_history = 0;
        m_bucket_ranges_buffer = nullptr;
        m_bucket_bases_buffer = nullptr;
        m_template_count = 0;
        m_generation_counter = 0;
        m_bucket_counts_buffer = nullptr;
        m_visible_count_readback_buffer = nullptr;
        m_culling_params_buffer = nullptr;
        m_visible_count_buffer = nullptr;
        m_visible_objects_buffer = nullptr;
        m_bucket_fill_binding_layout = nullptr;
        m_bucket_fill_pipeline = nullptr;
        m_bucket_scan_binding_layout = nullptr;
        m_bucket_scan_pipeline = nullptr;
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

        for (auto& templates : m_bucket_templates_buffer) {
            if (!templates) {
                templates = cmd_list.createBuffer(
                    GfxBufferDesc()
                        .setByteSize(kMaxBuckets * static_cast<UInt32>(sizeof(GpuBucketTemplate)))
                        .setStructStride(sizeof(GpuBucketTemplate))
                        .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                        .setDebugName("BucketTemplates"));
            }
        }

        for (auto& map_buffer : m_bucket_template_map_buffer) {
            if (!map_buffer) {
                map_buffer = cmd_list.createBuffer(
                    GfxBufferDesc()
                        .setByteSize(kMaxBuckets * static_cast<UInt32>(sizeof(UInt32)))
                        .setStructStride(sizeof(UInt32))
                        .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                        .setDebugName("BucketTemplateMap"));
            }
        }

        if (!m_bucket_bases_buffer) {
            m_bucket_bases_buffer = cmd_list.createBuffer(
                GfxBufferDesc()
                    .setByteSize(kMaxBuckets * static_cast<UInt32>(sizeof(UInt32)))
                    .setStructStride(sizeof(UInt32))
                    .setCanHaveUAVs(true)
                    .enableAutomaticStateTracking(GfxResourceStates::UnorderedAccess)
                    .setDebugName("BucketBases"));
        }

        if (!m_bucket_ranges_buffer) {
            m_bucket_ranges_buffer = cmd_list.createBuffer(
                GfxBufferDesc()
                    .setByteSize(kMaxBuckets * static_cast<UInt32>(sizeof(GpuBucketRange)))
                    .setStructStride(sizeof(GpuBucketRange))
                    .setCanHaveUAVs(true)
                    .enableAutomaticStateTracking(GfxResourceStates::UnorderedAccess)
                    .setDebugName("BucketRanges"));
        }

        const UInt64 indirect_args_size = std::max(object_count, 1u) * static_cast<UInt32>(sizeof(DrawIndexedIndirectArgs));
        for (auto& indirect_args : m_indirect_args_buffer) {
            if (!indirect_args ||
                indirect_args->getByteSize() < indirect_args_size) {
                indirect_args = cmd_list.createBuffer(
                    GfxBufferDesc()
                        .setByteSize(indirect_args_size)
                        .setIsDrawIndirectArgs(true)
                        .setCanHaveUAVs(true)
                        .enableAutomaticStateTracking(GfxResourceStates::UnorderedAccess)
                        .setDebugName("IndirectArgs"));
            }
        }

        for (auto& readback : m_bucket_ranges_readback_buffer) {
            if (!readback) {
                readback = cmd_list.createBuffer(
                    GfxBufferDesc()
                        .setByteSize(kMaxBuckets * static_cast<UInt32>(sizeof(GpuBucketRange)))
                        .setStructStride(sizeof(GpuBucketRange))
                        .setCpuAccess(GfxCpuAccessMode::Read)
                        .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                        .setDebugName("BucketRangesReadback"));
            }
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
            layout_desc.setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::ConstantBuffer(0))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(1))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(2))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(3))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_UAV(4))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_UAV(5));
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
        binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(1, scene_resources.object_meta->getRHIHandle()));
        binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(2, scene_resources.transforms->getRHIHandle()));
        binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(3, scene_resources.bounds->getRHIHandle()));
        binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_UAV(4, m_visible_objects_buffer->getRHIHandle()));
        binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_UAV(5, m_visible_count_buffer->getRHIHandle()));

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

        m_object_count = effective_count;
    }

    void GpuCulling::executeBucketBuild(DrawCommandList& cmd_list,
                                         const GpuScenePassResources& scene_resources,
                                         UInt32 object_count) {
        if (!m_enabled || !m_shader_library || object_count == 0) return;
        if (!m_visible_objects_buffer || !m_visible_count_buffer) return;
        if (!scene_resources.primitive_instance || !scene_resources.primitive_instance->getRHI()) return;

        ensureBucketBuffers(cmd_list, object_count);

        const auto bucket_count_cs = m_shader_library->getBucketCountComputeShader();
        const auto bucket_scan_cs = m_shader_library->getBucketScanComputeShader();
        const auto bucket_fill_cs = m_shader_library->getBucketFillComputeShader();

        if (!m_bucket_count_binding_layout) {
            GfxBindingLayoutDesc layout_desc;
            layout_desc.setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(1))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(2))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(3))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(4))
                .addItem(GfxBindingLayoutItem::ConstantBuffer(0))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_UAV(5));
            m_bucket_count_binding_layout = cmd_list.createBindingLayout(layout_desc);
        }

        if (!m_bucket_count_pipeline && bucket_count_cs) {
            GfxComputePipelineDesc pipeline_desc;
            pipeline_desc.setComputeShader(bucket_count_cs);
            pipeline_desc.addBindingLayout(m_bucket_count_binding_layout);
            m_bucket_count_pipeline = m_gfx->getDevice()->createComputePipeline(pipeline_desc);
        }

        if (!m_bucket_scan_binding_layout) {
            GfxBindingLayoutDesc layout_desc;
            layout_desc.setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::ConstantBuffer(0))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(1))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_UAV(2));
            m_bucket_scan_binding_layout = cmd_list.createBindingLayout(layout_desc);
        }

        if (!m_bucket_scan_pipeline && bucket_scan_cs) {
            GfxComputePipelineDesc pipeline_desc;
            pipeline_desc.setComputeShader(bucket_scan_cs);
            pipeline_desc.addBindingLayout(m_bucket_scan_binding_layout);
            m_bucket_scan_pipeline = m_gfx->getDevice()->createComputePipeline(pipeline_desc);
        }

        if (!m_bucket_fill_binding_layout) {
            GfxBindingLayoutDesc layout_desc;
            layout_desc.setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::ConstantBuffer(0))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(1))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(2))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(3))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_UAV(4))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_UAV(5))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(7))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(8))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(10))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_SRV(11))
                .addItem(GfxBindingLayoutItem::StructuredBuffer_UAV(12));
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
            DynamicArray<UInt32> zero_buckets(kMaxBuckets * 4, 0);
            cmd_list.setBufferState(m_bucket_counts_buffer, GfxResourceStates::CopyDest);
            cmd_list.commitBarriers();
            cmd_list.writeBuffer(m_bucket_counts_buffer, zero_buckets.data(), bucket_size, 0);
            cmd_list.setBufferState(m_bucket_counts_buffer, GfxResourceStates::UnorderedAccess);
            cmd_list.commitBarriers();

            const UInt32 count_params[4] = {object_count, kMaxBuckets, m_template_count, 0};
            cmd_list.writeBuffer(m_culling_params_buffer, count_params, sizeof(count_params), 0);

            GfxBindingSetDesc count_binding_desc;
            count_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(1, m_visible_objects_buffer->getRHIHandle()));
            count_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(2, scene_resources.object_meta->getRHIHandle()));
            count_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(3, m_visible_count_buffer->getRHIHandle()));
            count_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(4, scene_resources.primitive_instance->getRHIHandle()));
            count_binding_desc.addItem(GfxBindingSetItem::ConstantBuffer(0, m_culling_params_buffer->getRHIHandle()));
            count_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_UAV(5, m_bucket_counts_buffer->getRHIHandle()));

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

        if (bucket_scan_cs && m_bucket_scan_pipeline) {
            GfxBindingSetDesc scan_binding_desc;
            scan_binding_desc.addItem(GfxBindingSetItem::ConstantBuffer(0, m_culling_params_buffer->getRHIHandle()));
            scan_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(1, m_bucket_counts_buffer->getRHIHandle()));
            scan_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_UAV(2, m_bucket_bases_buffer->getRHIHandle()));

            auto scan_binding_set = cmd_list.createBindingSet(scan_binding_desc, m_bucket_scan_binding_layout);

            GfxComputeState scan_state;
            scan_state.setPipeline(m_bucket_scan_pipeline);
            scan_state.addBindingSet(scan_binding_set->getRHIHandle());
            cmd_list.setComputeState(scan_state);
            cmd_list.dispatch(1, 1, 1);

            cmd_list.setBufferState(m_bucket_bases_buffer, GfxResourceStates::ShaderResource);
            cmd_list.commitBarriers();
        }

        if (bucket_fill_cs && m_bucket_fill_pipeline) {
            const UInt32 generation = m_generation_counter % kMaxGenerations;
            auto& indirect_args = m_indirect_args_buffer[generation];
            auto& templates = m_bucket_templates_buffer[generation];
            auto& template_map = m_bucket_template_map_buffer[generation];
            auto& ranges_readback = m_bucket_ranges_readback_buffer[generation];
            if (!indirect_args || !templates || !template_map || !ranges_readback) {
                return;
            }

            const UInt64 indirect_size = std::max(object_count, 1u) *
                static_cast<UInt32>(sizeof(DrawIndexedIndirectArgs));
            DynamicArray<UInt32> zero_indirect((indirect_size + sizeof(UInt32) - 1) / sizeof(UInt32), 0);
            cmd_list.setBufferState(indirect_args, GfxResourceStates::CopyDest);
            cmd_list.commitBarriers();
            cmd_list.writeBuffer(indirect_args, zero_indirect.data(), indirect_size, 0);

            const UInt32 ranges_size = kMaxBuckets * static_cast<UInt32>(sizeof(GpuBucketRange));
            DynamicArray<UInt32> zero_ranges(ranges_size / sizeof(UInt32), 0);
            cmd_list.setBufferState(m_bucket_ranges_buffer, GfxResourceStates::CopyDest);
            cmd_list.commitBarriers();
            cmd_list.writeBuffer(m_bucket_ranges_buffer, zero_ranges.data(), ranges_size, 0);

            cmd_list.setBufferState(indirect_args, GfxResourceStates::UnorderedAccess);
            cmd_list.setBufferState(m_bucket_counts_buffer, GfxResourceStates::UnorderedAccess);
            cmd_list.setBufferState(m_bucket_ranges_buffer, GfxResourceStates::UnorderedAccess);
            cmd_list.commitBarriers();

            GfxBindingSetDesc fill_binding_desc;
            fill_binding_desc.addItem(GfxBindingSetItem::ConstantBuffer(0, m_culling_params_buffer->getRHIHandle()));
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(1, m_visible_objects_buffer->getRHIHandle()));
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(2, scene_resources.object_meta->getRHIHandle()));
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(3, m_visible_count_buffer->getRHIHandle()));
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_UAV(4, m_bucket_counts_buffer->getRHIHandle()));
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_UAV(5, indirect_args->getRHIHandle()));
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(7, scene_resources.primitive_instance->getRHIHandle()));
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(8, templates->getRHIHandle()));
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(10, template_map->getRHIHandle()));
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_SRV(11, m_bucket_bases_buffer->getRHIHandle()));
            fill_binding_desc.addItem(GfxBindingSetItem::StructuredBuffer_UAV(12, m_bucket_ranges_buffer->getRHIHandle()));

            auto fill_binding_set = cmd_list.createBindingSet(fill_binding_desc, m_bucket_fill_binding_layout);

            GfxComputeState fill_state;
            fill_state.setPipeline(m_bucket_fill_pipeline);
            fill_state.addBindingSet(fill_binding_set->getRHIHandle());
            cmd_list.setComputeState(fill_state);

            const UInt32 fill_groups = (object_count + 63) / 64;
            cmd_list.dispatch(fill_groups, 1, 1);

            cmd_list.setBufferState(indirect_args, GfxResourceStates::IndirectArgument);
            cmd_list.setBufferState(m_bucket_ranges_buffer, GfxResourceStates::CopySource);
            cmd_list.setBufferState(ranges_readback, GfxResourceStates::CopyDest);
            cmd_list.commitBarriers();
            cmd_list.copyBuffer(ranges_readback, 0,
                m_bucket_ranges_buffer, 0, kMaxBuckets * static_cast<UInt32>(sizeof(GpuBucketRange)));

            m_generation_counter++;
        }
    }

    void GpuCulling::uploadBucketTemplates(DrawCommandList& cmd_list,
                                           const GpuBucketTemplateUpload* uploads,
                                           const UInt32 upload_count) {
        if (!uploads || upload_count == 0) {
            return;
        }
        const UInt32 generation = m_generation_counter % kMaxGenerations;
        auto& templates_buffer = m_bucket_templates_buffer[generation];
        auto& map_buffer = m_bucket_template_map_buffer[generation];
        if (!templates_buffer) {
            templates_buffer = cmd_list.createBuffer(
                GfxBufferDesc()
                    .setByteSize(kMaxBuckets * static_cast<UInt32>(sizeof(GpuBucketTemplate)))
                    .setStructStride(sizeof(GpuBucketTemplate))
                    .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                    .setDebugName("BucketTemplates"));
        }
        if (!map_buffer) {
            map_buffer = cmd_list.createBuffer(
                GfxBufferDesc()
                    .setByteSize(kMaxBuckets * static_cast<UInt32>(sizeof(UInt32)))
                    .setStructStride(sizeof(UInt32))
                    .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                    .setDebugName("BucketTemplateMap"));
        }

        const UInt32 count = std::min(upload_count, kMaxBuckets);
        DynamicArray<GpuBucketTemplate> templates(count);
        for (UInt32 index = 0; index < count; ++index) {
            templates[index] = uploads[index].data;
        }
        const UInt32 byte_size = count * static_cast<UInt32>(sizeof(GpuBucketTemplate));
        cmd_list.setBufferState(templates_buffer, GfxResourceStates::CopyDest);
        cmd_list.commitBarriers();
        cmd_list.writeBuffer(templates_buffer, templates.data(), byte_size, 0);
        cmd_list.setBufferState(templates_buffer, GfxResourceStates::ShaderResource);
        cmd_list.commitBarriers();

        auto& map_history = m_template_map_history[generation];
        map_history.assign(kMaxBuckets, kInvalidBucketTemplateIndex);
        for (UInt32 index = 0; index < count; ++index) {
            const UInt32 hash_bucket = uploads[index].hash_bucket;
            if (hash_bucket < kMaxBuckets &&
                map_history[hash_bucket] == kInvalidBucketTemplateIndex) {
                map_history[hash_bucket] = index;
            }
        }
        m_template_count_history[generation] = count;
        cmd_list.setBufferState(map_buffer, GfxResourceStates::CopyDest);
        cmd_list.commitBarriers();
        cmd_list.writeBuffer(map_buffer, map_history.data(),
            kMaxBuckets * static_cast<UInt32>(sizeof(UInt32)), 0);
        cmd_list.setBufferState(map_buffer, GfxResourceStates::ShaderResource);
        cmd_list.commitBarriers();

        m_template_count = count;
    }

    Bool GpuCulling::acquireBucketDraws(DynamicArray<GpuBucketCpuDraw>& out_draws) {
        out_draws.clear();
        if (!m_gfx || m_generation_counter < 2) {
            return false;
        }
        const UInt32 generation = m_generation_counter - 2;
        const UInt32 slot = generation % kMaxGenerations;
        const auto& readback = m_bucket_ranges_readback_buffer[slot];
        const auto& template_map = m_template_map_history[slot];
        if (!readback || template_map.size() != kMaxBuckets) {
            return false;
        }

        auto device = m_gfx->getDevice();
        void* mapped = device->mapBuffer(readback->getRHI(), GfxCpuAccessMode::Read);
        if (!mapped) {
            return false;
        }
        const auto* ranges = static_cast<const GpuBucketRange*>(mapped);
        for (UInt32 bucket = 0; bucket < kMaxBuckets; ++bucket) {
            const UInt32 template_index = template_map[bucket];
            if (ranges[bucket].arg_count > 0 && template_index != kInvalidBucketTemplateIndex) {
                out_draws.push_back(GpuBucketCpuDraw{
                    template_index, ranges[bucket].first_arg, ranges[bucket].arg_count});
            }
        }
        device->unmapBuffer(readback->getRHI());

        std::stable_sort(out_draws.begin(), out_draws.end(),
            [](const GpuBucketCpuDraw& lhs, const GpuBucketCpuDraw& rhs) {
                return lhs.template_index < rhs.template_index;
            });
        return !out_draws.empty();
    }

    GpuVisibleStats GpuCulling::getLastVisibleStats() const {
        GpuVisibleStats stats{};
        stats.object_count = m_object_count;
        return stats;
    }

} // dodoe
