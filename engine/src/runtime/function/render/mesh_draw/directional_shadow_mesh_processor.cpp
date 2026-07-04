// do@Redlive

#include "directional_shadow_mesh_processor.h"

#include "mesh_draw_command.h"
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

    void DirectionalShadowMeshProcessor::buildCommands(
        const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
        const DynamicArray<MeshPassRelevance>& primitive_mesh_pass_relevance,
        const DynamicArray<UInt32>& mesh_pass_primitive_indices,
        const Matrix4f& light_view_projection,
        DynamicArray<MeshDrawCommand>& out_commands) const
    {
        (void)primitive_mesh_pass_relevance;
        out_commands.clear();
        out_commands.reserve(mesh_pass_primitive_indices.size());
        const auto frustum_planes = extractFrustumPlanes(light_view_projection);

        UInt32 first_instance = 0;
        for (const UInt32 primitive_index : mesh_pass_primitive_indices) {
            DO_ASSERT(primitive_index < visible_primitives.size(), "DirectionalShadowMeshProcessor primitive index out of range");
            const auto* primitive = visible_primitives[primitive_index];
            if (!primitive || !primitive->castsShadow()) {
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

                MeshDrawCommand command{};
                command.pass_type = MeshPassType::DirectionalShadow;
                command.primitive_index = static_cast<UInt32>(primitive_index);
                command.binding_sets.push_back(m_binding_set);
                command.vertex_bindings.push_back(GfxVertexBufferBinding().setBuffer(element.vertex_buffer->getRHI()).setSlot(0).setOffset(0));
                command.setPrimitiveSceneBufferBinding(1, static_cast<UInt64>(first_instance) * sizeof(InstanceSceneData));
                command.index_binding = GfxIndexBufferBinding()
                    .setBuffer(element.index_buffer->getRHI())
                    .setFormat(GfxFormat::R32_UINT)
                    .setOffset(0);
                command.draw_args = GfxDrawArguments()
                    .setVertexCount(element.index_count)
                    .setInstanceCount(element.instance_count)
                    .setStartIndexLocation(element.index_offset)
                    .setStartVertexLocation(element.vertex_offset);
                command.sort_key = reinterpret_cast<UInt64>(element.vertex_buffer.get());
                out_commands.push_back(command);
            }

            first_instance += primitive->getInstanceCount();
        }

        std::sort(out_commands.begin(), out_commands.end(), [](const MeshDrawCommand& lhs, const MeshDrawCommand& rhs) {
            return lhs.sort_key < rhs.sort_key;
        });
    }

} // dodoe
