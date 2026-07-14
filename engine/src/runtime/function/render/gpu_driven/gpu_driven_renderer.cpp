// do@Redlive

#include "gpu_driven_renderer.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    static constexpr UInt32 kMaxObjects = 4096;

    Bool GpuCulling::initialize(const GpuCullingCreateInfo& info) {
        m_gfx = info.gfx_context;
        m_shader_library = info.shader_library;
        m_enabled = RenderSettings::IsGpuDrivenSupported();
        return true;
    }

    void GpuCulling::shutdown() {
        m_indirect_args_buffer = nullptr;
        m_culling_params_buffer = nullptr;
        m_visible_count_buffer = nullptr;
        m_visible_objects_buffer = nullptr;
        m_culling_binding_layout = nullptr;
        m_culling_pipeline = nullptr;
        m_shader_library = nullptr;
        m_gfx = nullptr;
        m_enabled = false;
    }

    void GpuCulling::executeCulling(DrawCommandList& cmd_list,
                                           const GpuScenePassResources& scene_resources,
                                           const Matrix4f& view_projection) {
        if (!m_enabled || !m_shader_library) return;

        const auto cs = m_shader_library->getGpuCullingComputeShader();
        if (!cs) return;

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

        if (!m_visible_objects_buffer) {
            m_visible_objects_buffer = cmd_list.createBuffer(
                GfxBufferDesc()
                    .setByteSize(kMaxObjects * sizeof(UInt32))
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

        // Extract frustum planes from view-projection matrix
        CullingParams params{};
        const Matrix4f& m = view_projection;
        params.frustum_planes[0] = Vector4f(m[3] + m[0]); // left
        params.frustum_planes[1] = Vector4f(m[3] - m[0]); // right
        params.frustum_planes[2] = Vector4f(m[3] + m[1]); // bottom
        params.frustum_planes[3] = Vector4f(m[3] - m[1]); // top
        params.frustum_planes[4] = Vector4f(m[3] + m[2]); // near
        params.frustum_planes[5] = Vector4f(m[3] - m[2]); // far
        params.object_count = kMaxObjects;

        cmd_list.setBufferState(m_culling_params_buffer, GfxResourceStates::CopyDest);
        cmd_list.commitBarriers();
        cmd_list.writeBuffer(m_culling_params_buffer, &params, sizeof(CullingParams), 0);
        cmd_list.setBufferState(m_culling_params_buffer, GfxResourceStates::ConstantBuffer);
        cmd_list.commitBarriers();

        // Zero visible count
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

        const UInt32 thread_groups = (kMaxObjects + 63) / 64;
        cmd_list.dispatch(thread_groups, 1, 1);

        if (!m_indirect_args_buffer) {
            m_indirect_args_buffer = cmd_list.createBuffer(
                GfxBufferDesc()
                    .setByteSize(kMaxObjects * sizeof(UInt32) * 5)
                    .setIsDrawIndirectArgs(true)
                    .setCanHaveUAVs(true)
                    .enableAutomaticStateTracking(GfxResourceStates::UnorderedAccess)
                    .setDebugName("IndirectArgs"));
        }

        cmd_list.setBufferState(m_indirect_args_buffer, GfxResourceStates::IndirectArgument);
        cmd_list.setBufferState(m_visible_objects_buffer, GfxResourceStates::ShaderResource);
        cmd_list.setBufferState(m_visible_count_buffer, GfxResourceStates::ShaderResource);
        cmd_list.commitBarriers();

        m_frame_index = (m_frame_index + 1) % kMaxFramesInFlight;
    }

} // dodoe
