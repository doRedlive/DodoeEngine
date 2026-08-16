// do@Redlive

#include "directional_shadow_mesh_processor.h"

#include "mesh_draw_types.h"
#include "mesh_draw_list.h"
#include "cached_mesh_draw_command.h"
#include "runtime/core/math/math.h"
#include "../render_scene/primitive_render_object.h"
#include "../material/material_system.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/graphics/draw_command_list.h"
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

    DirectionalShadowMeshProcessor::DirectionalShadowMeshProcessor(
        SharedRenderService* shared_render_service)
        : m_shared_render_service(shared_render_service)
    {
        DO_ASSERT(m_shared_render_service != nullptr, "DirectionalShadowMeshProcessor shared_render_service is null");
        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        auto* binding_set_cache = m_shared_render_service->getBindingSetCache();
        DO_ASSERT(binding_layout_cache != nullptr, "DirectionalShadowMeshProcessor binding_layout_cache is null");
        DO_ASSERT(binding_set_cache != nullptr, "DirectionalShadowMeshProcessor binding_set_cache is null");
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
        m_global_constant_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(GlobalMeshShaderData)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("DirectionalShadowMeshProcessor Global ConstantBuffer"));
        m_view_constant_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(ViewMeshShaderData)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("DirectionalShadowMeshProcessor View ConstantBuffer"));
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
    }

    void DirectionalShadowMeshProcessor::reset() {
        m_global_constant_buffer = nullptr;
        m_view_constant_buffer = nullptr;
        m_global_binding_set = nullptr;
        m_view_binding_set = nullptr;
        m_global_binding_layout = nullptr;
        m_view_binding_layout = nullptr;
        m_shared_render_service = nullptr;
        m_shadow_input_layout = nullptr;
        m_shadow_framebuffer_info = {};
    }

    void DirectionalShadowMeshProcessor::updateFrameData(GfxInputLayoutHandle shadow_input_layout,
                                                         GfxFramebufferInfo shadow_framebuffer_info) {
        m_shadow_input_layout = std::move(shadow_input_layout);
        m_shadow_framebuffer_info = std::move(shadow_framebuffer_info);
    }

    GfxGraphicsPipelineHandle DirectionalShadowMeshProcessor::resolvePipelineFor(
        const MaterialInstance* material_instance,
        DrawCommandList& cmd_list) const {
        auto* shader_library = m_shared_render_service->getShaderLibrary();
        auto* pso_cache = m_shared_render_service->getPipelineStateCache();
        DO_ASSERT(shader_library != nullptr, "DirectionalShadowMeshProcessor shader library is null");
        DO_ASSERT(pso_cache != nullptr, "DirectionalShadowMeshProcessor PSO cache is null");
        DO_ASSERT(m_shadow_input_layout, "DirectionalShadowMeshProcessor input layout is null");

        GfxShaderHandle vs = shader_library->getShadowVertexShader();
        GfxShaderHandle ps = shader_library->getShadowPixelShader();
        if (material_instance && material_instance->tpl) {
            if (material_instance->tpl->vertex_shader) vs = material_instance->tpl->vertex_shader;
        }

        auto pipeline_desc = GfxGraphicsPipelineDesc()
            .setVertexShader(vs)
            .setPixelShader(ps)
            .setInputLayout(m_shadow_input_layout)
            .addBindingLayout(m_global_binding_layout)
            .addBindingLayout(m_view_binding_layout)
            .setPrimType(GfxPrimitiveType::TriangleList);
        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(GfxComparisonFunc::Less).disableStencil();
        GfxRasterState raster_state;
        raster_state.setCullBack().setDepthBiasClamp(0.0f).setDepthBias(6).setSlopeScaleDepthBias(1.5f);
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state);
        render_state.setRasterState(raster_state);
        pipeline_desc.setRenderState(render_state);

        return pso_cache->resolveGraphicsPipeline(
            MeshPassType::DirectionalShadow, pipeline_desc, m_shadow_framebuffer_info, cmd_list);
    }

    void DirectionalShadowMeshProcessor::buildCachedCommands(
        const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
        const DynamicArray<MeshPassRelevance>& primitive_mesh_pass_relevance,
        const DynamicArray<UInt32>& mesh_pass_primitive_indices,
        const Matrix4f& view_projection,
        MeshDrawCommandCache& cache,
        DynamicArray<MeshDrawInstance>& out_instances,
        DrawCommandList& cmd_list) const
    {
        (void)primitive_mesh_pass_relevance;
        out_instances.clear();
        out_instances.reserve(mesh_pass_primitive_indices.size());

        const auto frustum_planes = ExtractFrustumPlanes(view_projection);

        UInt32 first_instance = 0;
        for (const UInt32 primitive_index : mesh_pass_primitive_indices) {
            DO_ASSERT(primitive_index < visible_primitives.size(), "DirectionalShadowMeshProcessor primitive index out of range");
            const auto* primitive = visible_primitives[primitive_index];
            if (!primitive || !primitive->castsShadow() || primitive->getMobility() == PrimitiveMobility::Movable) {
                if (primitive) {
                    first_instance += primitive->getInstanceCount();
                }
                continue;
            }
            const auto& batches = primitive->getMeshBatches();
            for (const auto& batch : batches) {
                if (!batch.isValid() || !batch.isRelevant(MeshPassType::DirectionalShadow) || batch.elements.empty()) {
                    continue;
                }
                if (IsBatchFrustumCulled(batch, primitive, frustum_planes)) {
                    continue;
                }
                const auto& element = batch.elements[0];
                if (!element.isValid()) {
                    continue;
                }

                const auto cache_key = CacheHashUtils::MakeCacheKey(
                    element, batch.material_instance, MeshPassType::DirectionalShadow);

                auto cmd = BuildDrawCommand(element, MeshPassType::DirectionalShadow, GfxBindingSetHandle{});
                cmd.setMaterialInstance(const_cast<MaterialInstance*>(batch.material_instance));
                cmd.setPipeline(resolvePipelineFor(batch.material_instance, cmd_list));
                cmd.setBindingSet(ShaderParameterSet::Global, m_global_binding_set);
                cmd.setBindingSet(ShaderParameterSet::View, m_view_binding_set);
                const UInt32 cmd_index = cache.findOrCreate(cache_key, std::move(cmd));

                MeshDrawInstance instance{};
                instance.cmd_index = cmd_index;
                instance.instance_offset = static_cast<UInt64>(first_instance) * sizeof(InstanceSceneData);
                out_instances.push_back(instance);
            }

            first_instance += primitive->getInstanceCount();
        }
    }

    void DirectionalShadowMeshProcessor::buildDynamicCommands(
        const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
        const DynamicArray<MeshPassRelevance>& primitive_mesh_pass_relevance,
        const DynamicArray<UInt32>& mesh_pass_primitive_indices,
        const Matrix4f& light_view_projection,
        DynamicArray<MeshDrawCommand>& frame_commands,
        DynamicArray<MeshDrawInstance>& out_instances,
        DrawCommandList& cmd_list) const
    {
        (void)primitive_mesh_pass_relevance;
        out_instances.clear();
        out_instances.reserve(mesh_pass_primitive_indices.size());

        DynamicArray<MeshDrawCommand> local_commands;
        local_commands.reserve(mesh_pass_primitive_indices.size());

        const auto frustum_planes = ExtractFrustumPlanes(light_view_projection);

        UInt32 first_instance = 0;
        for (const UInt32 primitive_index : mesh_pass_primitive_indices) {
            DO_ASSERT(primitive_index < visible_primitives.size(), "DirectionalShadowMeshProcessor primitive index out of range");
            const auto* primitive = visible_primitives[primitive_index];
            if (!primitive || !primitive->castsShadow() || primitive->getMobility() != PrimitiveMobility::Movable) {
                if (primitive) {
                    first_instance += primitive->getInstanceCount();
                }
                continue;
            }

            const auto& batches = primitive->getMeshBatches();
            for (const auto& batch : batches) {
                if (!batch.isValid() || !batch.isRelevant(MeshPassType::DirectionalShadow) || batch.elements.empty()) {
                    continue;
                }
                if (IsBatchFrustumCulled(batch, primitive, frustum_planes)) {
                    continue;
                }
                const auto& element = batch.elements[0];
                if (!element.isValid()) {
                    continue;
                }

                auto cmd = BuildDrawCommand(element, MeshPassType::DirectionalShadow, GfxBindingSetHandle{});
                cmd.setMaterialInstance(const_cast<MaterialInstance*>(batch.material_instance));
                cmd.setPipeline(resolvePipelineFor(batch.material_instance, cmd_list));
                cmd.setBindingSet(ShaderParameterSet::Global, m_global_binding_set);
                cmd.setBindingSet(ShaderParameterSet::View, m_view_binding_set);
                const UInt32 cmd_index = static_cast<UInt32>(local_commands.size());
                local_commands.push_back(std::move(cmd));

                MeshDrawInstance instance{};
                instance.cmd_index = cmd_index;
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
