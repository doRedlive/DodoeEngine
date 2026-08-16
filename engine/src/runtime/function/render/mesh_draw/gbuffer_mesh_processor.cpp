// do@Redlive

#include "gbuffer_mesh_processor.h"

#include "mesh_draw_types.h"
#include "mesh_draw_list.h"
#include "cached_mesh_draw_command.h"
#include "runtime/core/math/math.h"
#include "../render_scene/primitive_render_object.h"
#include "../material/material_system.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/binding_set_cache.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/pipeline/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"

namespace dodoe {
    namespace {
        constexpr UInt32 kVolatileConstantBufferVersions = 4096;
    }

    GBufferMeshProcessor::GBufferMeshProcessor(SharedRenderService* shared_render_service)
        : m_shared_render_service(shared_render_service) {
        DO_ASSERT(m_shared_render_service != nullptr, "GBufferMeshProcessor shared_render_service is null");
        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        auto* binding_set_cache = m_shared_render_service->getBindingSetCache();
        auto* descriptor_table = m_shared_render_service->getDescriptorTable();
        DO_ASSERT(binding_layout_cache != nullptr, "GBufferMeshProcessor binding_layout_cache is null");
        DO_ASSERT(binding_set_cache != nullptr, "GBufferMeshProcessor binding_set_cache is null");
        if (descriptor_table && descriptor_table->getDescriptorTable()) {
            m_descriptor_binding_set = create_ref<GfxBindingSet>(
                cutie::BindingSetHandle(descriptor_table->getDescriptorTable()));
        }
        m_sampler = GDrawCommandList.createSampler(GfxSamplerDesc());
        m_global_binding_layout = binding_layout_cache.getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Global))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kGlobalBindingConstants)));
        m_view_binding_layout = binding_layout_cache.getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::View))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kViewBindingConstants)));
        m_primitive_binding_layout = binding_layout_cache.getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Primitive))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kPrimitiveBindingConstants)));
        m_sampler_binding_layout = binding_layout_cache.getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Material))
                .addItem(GfxBindingLayoutItem::Sampler(shader_bindings::kMaterialBindingSampler)));
        m_global_constant_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(GlobalMeshShaderData)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("GBufferMeshProcessor Global ConstantBuffer"));
        m_view_constant_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(ViewMeshShaderData)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("GBufferMeshProcessor View ConstantBuffer"));
        m_primitive_constant_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(PrimitiveMeshDrawShaderData)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("GBufferMeshProcessor Primitive ConstantBuffer"));
        m_global_binding_set = binding_set_cache.getOrCreate(
            GfxBindingSetDesc()
                .addItem(GfxBindingSetItem::ConstantBuffer(shader_bindings::kGlobalBindingConstants, m_global_constant_buffer->getRHIHandle())),
            m_global_binding_layout,
            binding_layout_cache.getLayoutGeneration(m_global_binding_layout));
        m_view_binding_set = binding_set_cache.getOrCreate(
            GfxBindingSetDesc()
                .addItem(GfxBindingSetItem::ConstantBuffer(shader_bindings::kViewBindingConstants, m_view_constant_buffer->getRHIHandle())),
            m_view_binding_layout,
            binding_layout_cache.getLayoutGeneration(m_view_binding_layout));
        m_primitive_binding_set = binding_set_cache.getOrCreate(
            GfxBindingSetDesc()
                .addItem(GfxBindingSetItem::ConstantBuffer(shader_bindings::kPrimitiveBindingConstants, m_primitive_constant_buffer->getRHIHandle())),
            m_primitive_binding_layout,
            binding_layout_cache.getLayoutGeneration(m_primitive_binding_layout));
        m_sampler_binding_set = binding_set_cache.getOrCreate(
            GfxBindingSetDesc()
                .addItem(GfxBindingSetItem::Sampler(shader_bindings::kMaterialBindingSampler, m_sampler)),
            m_sampler_binding_layout,
            binding_layout_cache.getLayoutGeneration(m_sampler_binding_layout));
    }

    void GBufferMeshProcessor::reset() {
        m_global_constant_buffer = nullptr;
        m_view_constant_buffer = nullptr;
        m_primitive_constant_buffer = nullptr;
        m_global_binding_set = nullptr;
        m_view_binding_set = nullptr;
        m_primitive_binding_set = nullptr;
        m_sampler_binding_set = nullptr;
        m_global_binding_layout = nullptr;
        m_view_binding_layout = nullptr;
        m_primitive_binding_layout = nullptr;
        m_sampler_binding_layout = nullptr;
        m_sampler = nullptr;
        m_shared_render_service = nullptr;
        m_gbuffer_input_layout = nullptr;
        m_gbuffer_framebuffer_info = {};
    }

    void GBufferMeshProcessor::updateFrameData(GfxInputLayoutHandle gbuffer_input_layout,
                                                GfxFramebufferInfo gbuffer_framebuffer_info) {
        m_gbuffer_input_layout = std::move(gbuffer_input_layout);
        m_gbuffer_framebuffer_info = std::move(gbuffer_framebuffer_info);
    }

    GfxGraphicsPipelineHandle GBufferMeshProcessor::resolvePipelineFor(
        const MaterialInstance* material_instance,
        DrawCommandList& cmd_list) const {
        auto* shader_library = m_shared_render_service->getShaderLibrary();
        auto* pso_cache = m_shared_render_service->getPipelineStateCache();
        auto* descriptor_table = m_shared_render_service->getDescriptorTable();
        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        DO_ASSERT(shader_library != nullptr, "GBufferMeshProcessor shader library is null");
        DO_ASSERT(pso_cache != nullptr, "GBufferMeshProcessor PSO cache is null");
        DO_ASSERT(m_gbuffer_input_layout, "GBufferMeshProcessor input layout is null");

        GfxShaderHandle vs = shader_library->getGBufferVertexShader();
        GfxShaderHandle ps = shader_library->getGBufferPixelShader();
        GfxBindingLayoutHandle material_layout_handle;

        if (material_instance && material_instance->tpl) {
            if (material_instance->tpl->vertex_shader) vs = material_instance->tpl->vertex_shader;
            if (material_instance->tpl->pixel_shader)  ps = material_instance->tpl->pixel_shader;
        }
        if (material_instance && material_instance->material_binding_layout) {
            material_layout_handle = material_instance->material_binding_layout;
        } else if (RenderSettings::IsBindlessActive()) {
            material_layout_handle = m_sampler_binding_layout;
        } else {
            material_layout_handle = binding_layout_cache->getOrCreate(
                GfxBindingLayoutDesc()
                    .setVisibility(GfxShaderType::Pixel)
                    .setRegisterSpaceIsDescriptorSet(true)
                    .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Material))
                    .addItem(GfxBindingLayoutItem::Sampler(1))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(2))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(3)));
        }

        auto pipeline_desc = GfxGraphicsPipelineDesc()
            .setVertexShader(vs)
            .setPixelShader(ps)
            .setInputLayout(m_gbuffer_input_layout)
            .addBindingLayout(m_global_binding_layout)
            .addBindingLayout(m_view_binding_layout);
        pipeline_desc.addBindingLayout(material_layout_handle);
        pipeline_desc.addBindingLayout(m_primitive_binding_layout);
        if (RenderSettings::IsBindlessActive() && descriptor_table && descriptor_table->getDescriptorTable()) {
            pipeline_desc.addBindingLayout(descriptor_table->getDescriptorTable()->getLayout());
        }
        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(GfxComparisonFunc::Less).disableStencil();
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state);
        pipeline_desc.setRenderState(render_state);

        return pso_cache->resolveGraphicsPipeline(
            MeshPassType::GBuffer, pipeline_desc, m_gbuffer_framebuffer_info, cmd_list);
    }

    void GBufferMeshProcessor::buildCachedCommands(
        const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
        const DynamicArray<MeshPassRelevance>& primitive_mesh_pass_relevance,
        const DynamicArray<UInt32>& mesh_pass_primitive_indices,
        const Matrix4f& view_projection,
        MeshDrawCommandCache& cache,
        DynamicArray<MeshDrawInstance>& out_instances,
        DynamicArray<PrimitiveMeshDrawShaderData>& out_shader_data,
        DrawCommandList& cmd_list) const
    {
        (void)primitive_mesh_pass_relevance;
        out_instances.clear();
        out_shader_data.clear();
        out_instances.reserve(mesh_pass_primitive_indices.size());
        out_shader_data.reserve(mesh_pass_primitive_indices.size());

        const auto frustum_planes = ExtractFrustumPlanes(view_projection);

        UInt32 first_instance = 0;
        for (const UInt32 primitive_index : mesh_pass_primitive_indices) {
            DO_ASSERT(primitive_index < visible_primitives.size(), "GBufferMeshProcessor primitive index out of range");
            const auto* primitive = visible_primitives[primitive_index];
            if (!primitive || primitive->getMobility() == PrimitiveMobility::Movable) {
                continue;
            }
            const auto& batches = primitive->getMeshBatches();
            for (const auto& batch : batches) {
                if (!batch.isValid() || !batch.isRelevant(MeshPassType::GBuffer) || batch.elements.empty()) {
                    continue;
                }
                if (IsBatchFrustumCulled(batch, primitive, frustum_planes)) {
                    continue;
                }
                const auto& element = batch.elements[0];
                if (!element.isValid()) {
                    continue;
                }

                const auto* mi = batch.material_instance;
                PrimitiveMeshDrawShaderData draw_shader_data{};
                if (mi && !mi->texture_descriptor_indices.empty()) {
                    draw_shader_data.draw_data.x = mi->texture_descriptor_indices[0];
                    draw_shader_data.draw_data.y = mi->texture_descriptor_indices.size() > 1 ? mi->texture_descriptor_indices[1] : -1;
                    draw_shader_data.draw_data.z = mi->texture_descriptor_indices.size() > 1 ? 1 : 0;
                }

                const UInt32 shader_data_index = static_cast<UInt32>(out_shader_data.size());
                out_shader_data.push_back(draw_shader_data);

                const auto cache_key = CacheHashUtils::MakeCacheKey(
                    element, batch.material_instance, MeshPassType::GBuffer);

                auto cmd = BuildDrawCommand(element, MeshPassType::GBuffer, m_primitive_binding_set);
                cmd.setMaterialInstance(const_cast<MaterialInstance*>(mi));
                cmd.setPipeline(resolvePipelineFor(mi, cmd_list));
                cmd.setBindingSet(ShaderParameterSet::Global, m_global_binding_set);
                cmd.setBindingSet(ShaderParameterSet::View, m_view_binding_set);
                DO_ASSERT(mi != nullptr, "GBufferMeshProcessor batch has null material_instance");
                if (mi->material_binding_set) {
                    cmd.setBindingSet(ShaderParameterSet::Material, mi->material_binding_set);
                    if (RenderSettings::IsBindlessActive()) {
                        cmd.setBindingSet(ShaderParameterSet::Bindless, m_descriptor_binding_set);
                    }
                } else if (RenderSettings::IsBindlessActive()) {
                    cmd.setBindingSet(ShaderParameterSet::Material, m_sampler_binding_set);
                    cmd.setBindingSet(ShaderParameterSet::Bindless, m_descriptor_binding_set);
                } else {
                    DO_ASSERT(mi->texture_binding_set, "GBufferMeshProcessor material_instance has no texture_binding_set in non-bindless mode");
                    cmd.setBindingSet(ShaderParameterSet::Material, mi->texture_binding_set);
                }
                const UInt32 cmd_index = cache.findOrCreate(cache_key, std::move(cmd));

                MeshDrawInstance instance{};
                instance.cmd_index = cmd_index;
                instance.shader_data_index = shader_data_index;
                instance.instance_offset = static_cast<UInt64>(first_instance) * sizeof(InstanceSceneData);
                out_instances.push_back(instance);
            }

            first_instance += primitive->getInstanceCount();
        }
    }

    void GBufferMeshProcessor::buildDynamicCommands(
        const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
        const DynamicArray<MeshPassRelevance>& primitive_mesh_pass_relevance,
        const DynamicArray<UInt32>& mesh_pass_primitive_indices,
        const Matrix4f& view_projection,
        DynamicArray<MeshDrawCommand>& frame_commands,
        DynamicArray<MeshDrawInstance>& out_instances,
        DynamicArray<PrimitiveMeshDrawShaderData>& out_shader_data,
        DrawCommandList& cmd_list) const
    {
        (void)primitive_mesh_pass_relevance;
        out_instances.clear();
        out_shader_data.clear();
        out_instances.reserve(mesh_pass_primitive_indices.size());
        out_shader_data.reserve(mesh_pass_primitive_indices.size());

        DynamicArray<MeshDrawCommand> local_commands;
        local_commands.reserve(mesh_pass_primitive_indices.size());

        const auto frustum_planes = ExtractFrustumPlanes(view_projection);

        UInt32 first_instance = 0;
        for (const UInt32 primitive_index : mesh_pass_primitive_indices) {
            DO_ASSERT(primitive_index < visible_primitives.size(), "GBufferMeshProcessor primitive index out of range");
            const auto* primitive = visible_primitives[primitive_index];
            if (!primitive || primitive->getMobility() != PrimitiveMobility::Movable) {
                if (primitive) {
                    first_instance += primitive->getInstanceCount();
                }
                continue;
            }
            const auto& batches = primitive->getMeshBatches();
            for (const auto& batch : batches) {
                if (!batch.isValid() || !batch.isRelevant(MeshPassType::GBuffer) || batch.elements.empty()) {
                    continue;
                }
                if (IsBatchFrustumCulled(batch, primitive, frustum_planes)) {
                    continue;
                }
                const auto& element = batch.elements[0];
                if (!element.isValid()) {
                    continue;
                }

                const auto* mi = batch.material_instance;
                PrimitiveMeshDrawShaderData draw_shader_data{};
                if (mi && !mi->texture_descriptor_indices.empty()) {
                    draw_shader_data.draw_data.x = mi->texture_descriptor_indices[0];
                    draw_shader_data.draw_data.y = mi->texture_descriptor_indices.size() > 1 ? mi->texture_descriptor_indices[1] : -1;
                    draw_shader_data.draw_data.z = mi->texture_descriptor_indices.size() > 1 ? 1 : 0;
                }

                const UInt32 shader_data_index = static_cast<UInt32>(out_shader_data.size());
                out_shader_data.push_back(draw_shader_data);

                auto cmd = BuildDrawCommand(element, MeshPassType::GBuffer, m_primitive_binding_set);
                cmd.setMaterialInstance(const_cast<MaterialInstance*>(mi));
                cmd.setPipeline(resolvePipelineFor(mi, cmd_list));
                cmd.setBindingSet(ShaderParameterSet::Global, m_global_binding_set);
                cmd.setBindingSet(ShaderParameterSet::View, m_view_binding_set);
                DO_ASSERT(mi != nullptr, "GBufferMeshProcessor batch has null material_instance");
                if (mi->material_binding_set) {
                    cmd.setBindingSet(ShaderParameterSet::Material, mi->material_binding_set);
                    if (RenderSettings::IsBindlessActive()) {
                        cmd.setBindingSet(ShaderParameterSet::Bindless, m_descriptor_binding_set);
                    }
                } else if (RenderSettings::IsBindlessActive()) {
                    cmd.setBindingSet(ShaderParameterSet::Material, m_sampler_binding_set);
                    cmd.setBindingSet(ShaderParameterSet::Bindless, m_descriptor_binding_set);
                } else {
                    DO_ASSERT(mi->texture_binding_set, "GBufferMeshProcessor material_instance has no texture_binding_set in non-bindless mode");
                    cmd.setBindingSet(ShaderParameterSet::Material, mi->texture_binding_set);
                }
                const UInt32 cmd_index = static_cast<UInt32>(local_commands.size());
                local_commands.push_back(std::move(cmd));

                MeshDrawInstance instance{};
                instance.cmd_index = cmd_index;
                instance.shader_data_index = shader_data_index;
                instance.instance_offset = static_cast<UInt64>(first_instance) * sizeof(InstanceSceneData);
                out_instances.push_back(instance);
            }

            first_instance += primitive->getInstanceCount();
        }

        const UInt32 base_index = static_cast<UInt32>(frame_commands.size());
        frame_commands.reserve(frame_commands.size() + local_commands.size());
        for (auto& inst : out_instances) {
            inst.cmd_index += base_index;
        }
        for (auto& cmd : local_commands) {
            frame_commands.push_back(std::move(cmd));
        }
    }

} // namespace dodoe
