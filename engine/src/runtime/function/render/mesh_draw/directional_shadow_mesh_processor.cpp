// do@Redlive

#include "directional_shadow_mesh_processor.h"

#include "../render_object.h"

namespace dodoe {
    namespace {
        constexpr UInt32 kVolatileConstantBufferVersions = 4096;

        std::array<Vector4f, 6> extractFrustumPlanes(const Matrix4f& view_projection) {
            std::array<Vector4f, 6> planes{};
            const Matrix4f transposed = glm::transpose(view_projection);
            planes[0] = transposed[3] + transposed[0];
            planes[1] = transposed[3] - transposed[0];
            planes[2] = transposed[3] + transposed[1];
            planes[3] = transposed[3] - transposed[1];
            planes[4] = transposed[3] + transposed[2];
            planes[5] = transposed[3] - transposed[2];

            for (auto& plane : planes) {
                const Float length = glm::length(Vector3f(plane));
                if (length > std::numeric_limits<Float>::epsilon()) {
                    plane /= length;
                }
            }
            return planes;
        }

        Bool intersectsFrustum(const std::array<Vector4f, 6>& frustum_planes, const Vector3f& center, const Vector3f& extents) {
            for (const auto& plane : frustum_planes) {
                const Vector3f normal = Vector3f(plane);
                const Float radius = glm::dot(glm::abs(normal), extents);
                const Float distance = glm::dot(normal, center) + plane.w;
                if (distance + radius < 0.0f) {
                    return false;
                }
            }
            return true;
        }

    }

    void DirectionalShadowMeshProcessor::initialize(GfxContext& gfx_context) {
        const auto device = gfx_context.getDevice();
        DO_ASSERT(device != nullptr, "DirectionalShadowMeshProcessor device is null");

        m_binding_layout = device->createBindingLayout(
            BindingLayoutDesc()
                .setVisibility(ShaderType::All)
                .addItem(BindingLayoutItem::VolatileConstantBuffer(0))
        );
        m_constant_buffer = device->createBuffer(
            BufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(Matrix4f) + sizeof(Vector4f)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("DirectionalShadowMeshProcessor ConstantBuffer")
        );
        m_binding_set = device->createBindingSet(
            BindingSetDesc().addItem(BindingSetItem::ConstantBuffer(0, m_constant_buffer)),
            m_binding_layout
        );
    }

    void DirectionalShadowMeshProcessor::reset() {
        m_constant_buffer = nullptr;
        m_binding_set = nullptr;
        m_binding_layout = nullptr;
    }

    void DirectionalShadowMeshProcessor::buildCommands(
        const ViewMeshDrawContext& view_context,
        const Matrix4f& light_view_projection,
        ViewMeshDrawContext& out_view_context) const
    {
        auto& commands = out_view_context.getMeshPassCommands(MeshPassType::DirectionalShadow);
        const auto& visible_primitives = view_context.visible_primitives;
        const auto& relevant_primitive_indices = view_context.getMeshPassPrimitiveIndices(MeshPassType::DirectionalShadow);
        commands.clear();
        commands.reserve(relevant_primitive_indices.size());
        out_view_context.directional_shadow_view_projection = light_view_projection;
        const auto frustum_planes = extractFrustumPlanes(light_view_projection);

        for (const UInt32 primitive_index : relevant_primitive_indices) {
            DO_ASSERT(primitive_index < visible_primitives.size(), "DirectionalShadowMeshProcessor primitive index out of range");
            const auto* primitive = visible_primitives[primitive_index];
            if (!primitive) {
                continue;
            }

            const auto* render_object = primitive->getRenderObject();
            const UInt32 first_instance = primitive_index < view_context.primitive_first_instance_offsets.size()
                ? view_context.primitive_first_instance_offsets[primitive_index]
                : 0;
            const auto batches = render_object
                ? render_object->buildMeshBatches(primitive->getId(), primitive->getMaterials(), first_instance)
                : primitive->getMeshBatches();
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
                    const Matrix3f abs_linear(glm::abs(linear[0]), glm::abs(linear[1]), glm::abs(linear[2]));
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
                command.vertex_bindings.push_back(VertexBufferBinding().setBuffer(element.vertex_buffer).setSlot(0).setOffset(0));
                command.setPrimitiveSceneBufferBinding(1, static_cast<UInt64>(element.first_instance) * sizeof(InstanceSceneData));
                command.index_binding = IndexBufferBinding()
                    .setBuffer(element.index_buffer)
                    .setFormat(Format::R32_UINT)
                    .setOffset(0);
                command.draw_args = DrawArguments()
                    .setVertexCount(element.index_count)
                    .setInstanceCount(element.instance_count)
                    .setStartIndexLocation(element.index_offset)
                    .setStartVertexLocation(element.vertex_offset);
                command.sort_key = reinterpret_cast<UInt64>(element.vertex_buffer.Get());
                commands.push_back(command);
            }
        }

        std::sort(commands.begin(), commands.end(), [](const MeshDrawCommand& lhs, const MeshDrawCommand& rhs) {
            return lhs.sort_key < rhs.sort_key;
        });
    }

} // dodoe
