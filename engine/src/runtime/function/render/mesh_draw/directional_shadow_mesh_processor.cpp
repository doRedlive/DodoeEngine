// do@Redlive

#include "directional_shadow_mesh_processor.h"

#include "cached_mesh_draw_command.h"
#include "runtime/core/math/math.h"
#include "../render_scene/primitive_render_object.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {
    namespace {
        constexpr UInt32 kVolatileConstantBufferVersions = 4096;

        std::array<Vector4f, 6> extractFrustumPlanes(const Matrix4f& view_projection) {
            std::array<Vector4f, 6> planes{};
            const Matrix4f transposed = Math::Transpose(view_projection);
            planes[0] = transposed[3] + transposed[0];
            planes[1] = transposed[3] - transposed[0];
            planes[2] = transposed[3] + transposed[1];
            planes[3] = transposed[3] - transposed[1];
            planes[4] = transposed[3] + transposed[2];
            planes[5] = transposed[3] - transposed[2];

            for (auto& plane : planes) {
                const Float length = Math::Length(Vector3f(plane));
                if (length > std::numeric_limits<Float>::epsilon()) {
                    plane /= length;
                }
            }
            return planes;
        }

        Bool intersectsFrustum(const std::array<Vector4f, 6>& frustum_planes, const Vector3f& center, const Vector3f& extents) {
            for (const auto& plane : frustum_planes) {
                const Vector3f normal = Vector3f(plane);
                const Float radius = Math::Dot(Math::Abs(normal), extents);
                const Float distance = Math::Dot(normal, center) + plane.w;
                if (distance + radius < 0.0f) {
                    return false;
                }
            }
            return true;
        }

    }

    void DirectionalShadowMeshProcessor::initialize(GfxContext& gfx_context) {
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
        const auto frustum_planes = extractFrustumPlanes(light_view_projection);

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
                if (batch.uses_custom_bounds) {
                    const Vector3f local_center = (batch.bounds_min + batch.bounds_max) * 0.5f;
                    const Vector3f local_extents = (batch.bounds_max - batch.bounds_min) * 0.5f;
                    const Matrix4f& world_transform = primitive->getWorldTransform();
                    const Vector3f world_center = Vector3f(world_transform * Vector4f(local_center, 1.0f));
                    const Matrix3f linear = Matrix3f(world_transform);
                    const Matrix3f abs_linear(Math::Abs(linear[0]), Math::Abs(linear[1]), Math::Abs(linear[2]));
                    const Vector3f world_extents = abs_linear * local_extents;
                    if (!intersectsFrustum(frustum_planes, world_center, world_extents)) {
                        continue;
                    }
                }
                const auto& element = batch.elements[0];
                if (!element.isValid()) {
                    continue;
                }

                const auto cache_key = CacheHashUtils::MakeCacheKey(
                    element, batch.material, 0, 0, MeshPassType::DirectionalShadow);

                MeshDrawCommand cached_cmd{};
                cached_cmd.pass_type = MeshPassType::DirectionalShadow;
                cached_cmd.binding_sets.push_back(m_binding_set);
                cached_cmd.vertex_bindings.push_back(
                    GfxVertexBufferBinding().setBuffer(element.vertex_buffer->getRHI()).setSlot(0).setOffset(0));
                cached_cmd.index_binding = GfxIndexBufferBinding()
                    .setBuffer(element.index_buffer->getRHI())
                    .setFormat(GfxFormat::R32_UINT)
                    .setOffset(0);
                cached_cmd.draw_args = GfxDrawArguments()
                    .setVertexCount(element.index_count)
                    .setInstanceCount(element.instance_count)
                    .setStartIndexLocation(element.index_offset)
                    .setStartVertexLocation(element.vertex_offset);

                const UInt32 cmd_index = cache.findOrCreate(cache_key, std::move(cached_cmd));

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

        const auto frustum_planes = extractFrustumPlanes(light_view_projection);

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
                if (batch.uses_custom_bounds) {
                    const Vector3f local_center = (batch.bounds_min + batch.bounds_max) * 0.5f;
                    const Vector3f local_extents = (batch.bounds_max - batch.bounds_min) * 0.5f;
                    const Matrix4f& world_transform = primitive->getWorldTransform();
                    const Vector3f world_center = Vector3f(world_transform * Vector4f(local_center, 1.0f));
                    const Matrix3f linear = Matrix3f(world_transform);
                    const Matrix3f abs_linear(Math::Abs(linear[0]), Math::Abs(linear[1]), Math::Abs(linear[2]));
                    const Vector3f world_extents = abs_linear * local_extents;
                    if (!intersectsFrustum(frustum_planes, world_center, world_extents)) {
                        continue;
                    }
                }
                const auto& element = batch.elements[0];
                if (!element.isValid()) {
                    continue;
                }

                MeshDrawCommand cmd{};
                cmd.pass_type = MeshPassType::DirectionalShadow;
                cmd.binding_sets.push_back(m_binding_set);
                cmd.vertex_bindings.push_back(
                    GfxVertexBufferBinding().setBuffer(element.vertex_buffer->getRHI()).setSlot(0).setOffset(0));
                cmd.index_binding = GfxIndexBufferBinding()
                    .setBuffer(element.index_buffer->getRHI())
                    .setFormat(GfxFormat::R32_UINT)
                    .setOffset(0);
                cmd.draw_args = GfxDrawArguments()
                    .setVertexCount(element.index_count)
                    .setInstanceCount(element.instance_count)
                    .setStartIndexLocation(element.index_offset)
                    .setStartVertexLocation(element.vertex_offset);

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

} // dodoe

