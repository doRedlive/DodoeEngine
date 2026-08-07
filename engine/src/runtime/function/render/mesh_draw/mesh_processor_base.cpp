// do@Redlive

#include "mesh_processor_base.h"

#include "mesh_batch.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "../render_scene/primitive_scene_info.h"

namespace dodoe {

    StaticArray<Vector4f, 6> IMeshPassProcessor::ExtractFrustumPlanes(const Matrix4f& view_projection) {
        StaticArray<Vector4f, 6> planes{};
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

    Bool IMeshPassProcessor::IntersectsFrustum(const StaticArray<Vector4f, 6>& frustum_planes,
                                                const Vector3f& center, const Vector3f& extents) {
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

    Bool IMeshPassProcessor::IsBatchFrustumCulled(const MeshBatch& batch,
                                                   const PrimitiveSceneInfo* primitive,
                                                   const StaticArray<Vector4f, 6>& frustum_planes) {
        if (!batch.uses_custom_bounds) {
            return false;
        }
        const Vector3f local_center = (batch.bounds_min + batch.bounds_max) * 0.5f;
        const Vector3f local_extents = (batch.bounds_max - batch.bounds_min) * 0.5f;
        const Matrix4f& world_transform = primitive->getWorldTransform();
        const Vector3f world_center = Vector3f(world_transform * Vector4f(local_center, 1.0f));
        const Matrix3f linear = Matrix3f(world_transform);
        const Matrix3f abs_linear(Math::Abs(linear[0]), Math::Abs(linear[1]), Math::Abs(linear[2]));
        const Vector3f world_extents = abs_linear * local_extents;
        return !IntersectsFrustum(frustum_planes, world_center, world_extents);
    }

    MeshDrawCommand IMeshPassProcessor::BuildDrawCommand(const MeshBatchElement& element,
                                                         const MeshPassType pass_type,
                                                         const GfxBindingSetHandle& primitive_binding_set) {
        MeshDrawCommand cmd{};
        cmd.setPassType(pass_type);
        cmd.setBindingSet(ShaderParameterSet::Primitive, primitive_binding_set);
        cmd.addVertexBinding(
            GfxVertexBufferBinding().setBuffer(element.vertex_buffer->getRHI()).setSlot(0).setOffset(0));
        cmd.setIndexBinding(GfxIndexBufferBinding()
            .setBuffer(element.index_buffer->getRHI())
            .setFormat(GfxFormat::R32_UINT)
            .setOffset(0));
        cmd.setDrawArguments(GfxDrawArguments()
            .setVertexCount(element.index_count)
            .setInstanceCount(element.instance_count)
            .setStartIndexLocation(element.index_offset)
            .setStartVertexLocation(element.vertex_offset));
        return cmd;
    }

    void SubmitMeshDrawCommands(
        const DynamicArray<MeshDrawInstance>& instances,
        const DynamicArray<MeshDrawCommand>& commands,
        const DynamicArray<PrimitiveMeshDrawShaderData>& shader_data,
        const GfxBufferHandle& primitive_cb,
        const GfxFramebufferHandle& framebuffer,
        const GfxViewportState& viewport_state,
        const GfxBufferHandle& primitive_scene_buffer,
        DrawCommandList& command_list)
    {
        if (instances.empty()) return;

        ShaderParameterBinder binder;
        for (const auto& instance : instances) {
            DO_ASSERT(instance.cmd_index < commands.size(),
                "SubmitMeshDrawCommands cmd_index out of range");
            const auto& cmd = commands[instance.cmd_index];

            if (!cmd.getPipeline()) continue;

            if (primitive_cb && instance.hasShaderData() &&
                instance.shader_data_index < shader_data.size()) {
                const auto& data = shader_data[instance.shader_data_index];
                command_list.writeBuffer(primitive_cb, &data, sizeof(data));
            }

            auto graphics_state = GfxGraphicsState()
                .setFramebuffer(framebuffer->getRHI())
                .setViewport(viewport_state)
                .setPipeline(cmd.getPipeline()->getRHIHandle());

            binder.bind(graphics_state, cmd.getBindingSets());

            for (const auto& vertex_binding : cmd.getVertexBindings()) {
                graphics_state.addVertexBuffer(vertex_binding);
            }
            if (primitive_scene_buffer) {
                graphics_state.addVertexBuffer(
                    GfxVertexBufferBinding()
                        .setBuffer(primitive_scene_buffer->getRHI())
                        .setSlot(1)
                        .setOffset(instance.instance_offset)
                );
            }

            graphics_state.setIndexBuffer(cmd.getIndexBinding());
            command_list.setGraphicsState(graphics_state);
            command_list.drawIndexed(cmd.getDrawArguments());
        }
    }

} // namespace dodoe
