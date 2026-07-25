// do@Redlive

#include "gbuffer_mesh_processor.h"

#include "mesh_draw_types.h"
#include "mesh_draw_list.h"
#include "cached_mesh_draw_command.h"
#include "runtime/core/math/math.h"
#include "../render_scene/primitive_render_object.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {
    namespace {
        constexpr UInt32 kVolatileConstantBufferVersions = 4096;
    }

    GBufferMeshProcessor::GBufferMeshProcessor(GfxBindingSetHandle descriptor_binding_set)
        : m_descriptor_binding_set(std::move(descriptor_binding_set)) {
        m_sampler = GDrawCommandList.createSampler(GfxSamplerDesc());
        m_binding_layout = GDrawCommandList.createBindingLayout(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(0))
                .addItem(GfxBindingLayoutItem::Sampler(0))
        );
        m_constant_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(GBufferMeshDrawShaderData)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("GBufferMeshProcessor ConstantBuffer"));
        m_binding_set = GDrawCommandList.createBindingSet(
            GfxBindingSetDesc()
                .addItem(GfxBindingSetItem::ConstantBuffer(0, m_constant_buffer->getRHIHandle()))
                .addItem(GfxBindingSetItem::Sampler(0, m_sampler)),
            m_binding_layout
        );
    }

    void GBufferMeshProcessor::reset() {
        m_constant_buffer = nullptr;
        m_binding_set = nullptr;
        m_binding_layout = nullptr;
        m_sampler = nullptr;
    }

    void GBufferMeshProcessor::buildCachedCommands(
        const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
        const DynamicArray<MeshPassRelevance>& primitive_mesh_pass_relevance,
        const DynamicArray<UInt32>& mesh_pass_primitive_indices,
        const Matrix4f& view_projection,
        MeshDrawCommandCache& cache,
        DynamicArray<MeshDrawInstance>& out_instances,
        DynamicArray<GBufferMeshDrawShaderData>& out_shader_data) const
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

                GBufferMeshDrawShaderData draw_shader_data{};
                draw_shader_data.view_projection = view_projection;
                draw_shader_data.time_data = Vector4f(0.0f);
                const auto* mi = batch.material_instance;
                draw_shader_data.draw_data.x = mi->texture_descriptor_indices[0];
                draw_shader_data.draw_data.y = mi->texture_descriptor_indices.size() > 1 ? mi->texture_descriptor_indices[1] : -1;
                draw_shader_data.draw_data.z = mi->texture_descriptor_indices.size() > 1 ? 1 : 0;

                const UInt32 shader_data_index = static_cast<UInt32>(out_shader_data.size());
                out_shader_data.push_back(draw_shader_data);

                const auto cache_key = CacheHashUtils::MakeCacheKey(
                    element, batch.material_instance, MeshPassType::GBuffer);

                const UInt32 cmd_index = cache.findOrCreate(cache_key,
                    BuildDrawCommand(element, MeshPassType::GBuffer, m_binding_set));

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
        DynamicArray<GBufferMeshDrawShaderData>& out_shader_data) const
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

                GBufferMeshDrawShaderData draw_shader_data{};
                draw_shader_data.view_projection = view_projection;
                draw_shader_data.time_data = Vector4f(0.0f);
                const auto* mi = batch.material_instance;
                draw_shader_data.draw_data.x = mi->texture_descriptor_indices[0];
                draw_shader_data.draw_data.y = mi->texture_descriptor_indices.size() > 1 ? mi->texture_descriptor_indices[1] : -1;
                draw_shader_data.draw_data.z = mi->texture_descriptor_indices.size() > 1 ? 1 : 0;

                const UInt32 shader_data_index = static_cast<UInt32>(out_shader_data.size());
                out_shader_data.push_back(draw_shader_data);

                const UInt32 cmd_index = static_cast<UInt32>(local_commands.size());
                local_commands.push_back(BuildDrawCommand(element, MeshPassType::GBuffer, m_binding_set));

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
