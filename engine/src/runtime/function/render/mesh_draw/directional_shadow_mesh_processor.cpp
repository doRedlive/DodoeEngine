// do@Redlive

#include "directional_shadow_mesh_processor.h"

#include "cached_mesh_draw_command.h"
#include "../render_scene/primitive_render_object.h"
#include "runtime/function/graphics/gfx_context.h"
#include "mesh_draw_list.h"

namespace dodoe {
    namespace {
        constexpr UInt32 kVolatileConstantBufferVersions = 4096;
    }

    DirectionalShadowMeshProcessor::DirectionalShadowMeshProcessor() {
        m_binding_layout = GDrawCommandList.createBindingLayout(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(0))
        );
        m_constant_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(Matrix4f) + sizeof(Vector4f)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("DirectionalShadowMeshProcessor ConstantBuffer"));
        m_binding_set = GDrawCommandList.createBindingSet(
            GfxBindingSetDesc().addItem(GfxBindingSetItem::ConstantBuffer(0, m_constant_buffer->getRHIHandle())),
            m_binding_layout
        );
    }

    void DirectionalShadowMeshProcessor::reset() {
        m_constant_buffer = nullptr;
        m_binding_set = nullptr;
        m_binding_layout = nullptr;
    }

    void DirectionalShadowMeshProcessor::buildCachedCommands(
        const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
        const DynamicArray<MeshPassRelevance>& primitive_mesh_pass_relevance,
        const DynamicArray<UInt32>& mesh_pass_primitive_indices,
        const Matrix4f& light_view_projection,
        MeshDrawCommandCache& cache,
        DynamicArray<MeshDrawInstance>& out_instances) const
    {
        (void)primitive_mesh_pass_relevance;
        out_instances.clear();
        out_instances.reserve(mesh_pass_primitive_indices.size());
        const auto frustum_planes = ExtractFrustumPlanes(light_view_projection);

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

                const UInt32 cmd_index = cache.findOrCreate(cache_key,
                    BuildDrawCommand(element, MeshPassType::DirectionalShadow, m_binding_set));

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
        DynamicArray<MeshDrawInstance>& out_instances) const
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

                const UInt32 cmd_index = static_cast<UInt32>(local_commands.size());
                local_commands.push_back(
                    BuildDrawCommand(element, MeshPassType::DirectionalShadow, m_binding_set));

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
