// do@Redlive

#include "mesh_processor_base.h"

#include "mesh_batch.h"
#include "cached_mesh_draw_command.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/material/material_system.h"
#include "../render_scene/primitive_scene_info.h"

namespace dodoe {

    const GfxBindingLayoutHandle& MeshPassProcessor::getPrimitiveBindingLayout() const {
        static const GfxBindingLayoutHandle empty{};
        return empty;
    }

    const GfxBindingLayoutHandle& MeshPassProcessor::getSamplerBindingLayout() const {
        static const GfxBindingLayoutHandle empty{};
        return empty;
    }

    const GfxBufferHandle& MeshPassProcessor::getPrimitiveConstantBuffer() const {
        static const GfxBufferHandle empty{};
        return empty;
    }

    StaticArray<Vector4f, 6> MeshPassProcessor::ExtractFrustumPlanes(const Matrix4f& view_projection) {
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

    Bool MeshPassProcessor::IntersectsFrustum(const StaticArray<Vector4f, 6>& frustum_planes,
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

    Bool MeshPassProcessor::IsBatchFrustumCulled(const MeshBatch& batch,
                                                   const PrimitiveSceneInfo* primitive,
                                                   const StaticArray<Vector4f, 6>& frustum_planes) {
        if (!batch.usesCustomBounds()) {
            return false;
        }
        const Vector3f local_center = (batch.getBoundsMin() + batch.getBoundsMax()) * 0.5f;
        const Vector3f local_extents = (batch.getBoundsMax() - batch.getBoundsMin()) * 0.5f;
        const Matrix4f& world_transform = primitive->getWorldTransform();
        const Vector3f world_center = Vector3f(world_transform * Vector4f(local_center, 1.0f));
        const Matrix3f linear = Matrix3f(world_transform);
        const Matrix3f abs_linear(Math::Abs(linear[0]), Math::Abs(linear[1]), Math::Abs(linear[2]));
        const Vector3f world_extents = abs_linear * local_extents;
        return !IntersectsFrustum(frustum_planes, world_center, world_extents);
    }

    MeshDrawCommand MeshPassProcessor::BuildDrawCommand(const MeshBatchElement& element,
                                                         const GfxGraphicsPipelineHandle& pipeline) const {
        const auto draw_range = element.getDrawRange();
        const auto instance_range = element.getInstanceRange();
        MeshDrawCommand cmd{};
        cmd.setPassType(m_pass_type);
        cmd.setPipeline(pipeline);
        cmd.addVertexBinding(
            GfxVertexBufferBinding().setBuffer(element.vertex_buffer->getRHI()).setSlot(0).setOffset(0));
        cmd.setIndexBinding(GfxIndexBufferBinding()
            .setBuffer(element.index_buffer->getRHI())
            .setFormat(GfxFormat::R32_UINT)
            .setOffset(0));
        cmd.setDrawArguments(GfxDrawArguments()
            .setVertexCount(draw_range.index_count)
            .setInstanceCount(instance_range.instance_count)
            .setStartIndexLocation(draw_range.index_offset)
            .setStartVertexLocation(draw_range.vertex_offset));
        return cmd;
    }

    void MeshPassProcessor::buildMeshDrawCommands(const MeshPassCommandBuildContext& context) const {
        if (!context.pipeline) {
            return;
        }

        context.command_sources.reserve(
            context.command_sources.size() + context.mesh_pass_primitive_indices.size());

        const auto frustum_planes = ExtractFrustumPlanes(context.view_projection);
        UInt32 first_instance = 0;
        for (const UInt32 primitive_index : context.mesh_pass_primitive_indices) {
            DO_ASSERT(primitive_index < context.visible_primitives.size(),
                "MeshPassProcessor primitive index out of range");
            if (primitive_index >= context.visible_primitives.size()) {
                continue;
            }

            const auto* primitive = context.visible_primitives[primitive_index];
            if (!primitive) {
                continue;
            }

            const UInt32 primitive_first_instance = context.primitive_instance_offsets &&
                primitive_index < context.primitive_instance_offsets->size()
                ? (*context.primitive_instance_offsets)[primitive_index] : first_instance;
            first_instance += primitive->getInstanceCount();
            const Bool is_dynamic = primitive->getMobility() == PrimitiveMobility::Movable;
            if (!shouldDrawPrimitive(*primitive)) {
                continue;
            }
            if (primitive_index < context.primitive_mesh_pass_relevance.size() &&
                !context.primitive_mesh_pass_relevance[primitive_index].isRelevant(m_pass_type)) {
                continue;
            }

            const CommandLifetime lifetime = is_dynamic ? CommandLifetime::Frame : CommandLifetime::Cached;

            for (const auto& batch : primitive->getMeshBatches()) {
                if (!batch.isValid() || !batch.isRelevant(m_pass_type) ||
                    IsBatchFrustumCulled(batch, primitive, frustum_planes)) {
                    continue;
                }

                for (const auto& element : batch.getElements()) {
                    if (!element.isValid()) {
                        continue;
                    }

                    auto command = BuildDrawCommand(element, context.pipeline);
                    const auto* material = batch.getMaterialInstance();
                    command.setMaterialSortId(material
                        ? static_cast<Size_t>(std::hash<String>{}(material->desc.name)) : 0);
                    PrimitiveMeshDrawShaderData draw_shader_data{};
                    if (!setupMeshDrawCommand(batch, element, command, draw_shader_data)) {
                        continue;
                    }

                    MeshDrawCommandSource source{};
                    source.command = std::move(command);
                    const auto instance_range = element.getInstanceRange();
                    const UInt32 instance_base = primitive_first_instance +
                        (instance_range.explicit_range ? instance_range.first_instance : 0);
                    source.instance.instance_offset =
                        static_cast<UInt64>(instance_base) * sizeof(InstanceSceneData);
                    const Vector3f local_center = batch.usesCustomBounds()
                        ? (batch.getBoundsMin() + batch.getBoundsMax()) * 0.5f
                        : (primitive->getBoundsMin() + primitive->getBoundsMax()) * 0.5f;
                    const Vector3f world_center = Vector3f(
                        primitive->getWorldTransform() * Vector4f(local_center, 1.0f));
                    const Vector4f view_center = context.view_matrix * Vector4f(world_center, 1.0f);
                    source.instance.sort_depth = -view_center.z;
                    const Float normalized_depth =
                        std::max(0.0f, std::min(source.instance.sort_depth, 1000000.0f));
                    source.instance.depth_bucket = static_cast<UInt32>(normalized_depth * 1024.0f);
                    source.shader_data = draw_shader_data;
                    source.lifetime = lifetime;
                    if (lifetime == CommandLifetime::Cached) {
                        source.cache_key = CacheHashUtils::MakeCacheKey(
                            element, batch.getMaterialInstance(), m_pass_type, context.pipeline);
                        source.has_cache_key = true;
                    }
                    source.instance.cmd_index = static_cast<UInt32>(context.command_sources.size());
                    context.command_sources.push_back(std::move(source));
                }
            }
        }
    }

    void SubmitMeshDrawSources(
        const DynamicArray<MeshDrawCommandSource>& sources,
        const GfxBufferHandle& primitive_cb,
        const GfxFramebufferHandle& framebuffer,
        const GfxViewportState& viewport_state,
        const GfxBufferHandle& primitive_scene_buffer,
        const GfxBindingSetHandle* pass_binding_set,
        DrawCommandList& command_list)
    {
        if (sources.empty()) {
            return;
        }

        ShaderParameterBinder binder;
        for (const auto& source : sources) {
            const auto& cmd = source.command;
            if (!cmd.getPipeline()) {
                continue;
            }

            if (primitive_cb) {
                command_list.writeBuffer(primitive_cb, &source.shader_data,
                    sizeof(PrimitiveMeshDrawShaderData));
            }

            auto graphics_state = GfxGraphicsState()
                .setFramebuffer(framebuffer->getRHI())
                .setViewport(viewport_state)
                .setPipeline(cmd.getPipeline()->getRHIHandle());
            auto binding_sets = cmd.getBindingSets();
            if (pass_binding_set && *pass_binding_set) {
                binding_sets[static_cast<Size_t>(ShaderParameterSet::Pass)] = *pass_binding_set;
            }
            binder.bind(graphics_state, binding_sets);
            for (const auto& vertex_binding : cmd.getVertexBindings()) {
                graphics_state.addVertexBuffer(vertex_binding);
            }
            if (primitive_scene_buffer) {
                graphics_state.addVertexBuffer(
                    GfxVertexBufferBinding()
                        .setBuffer(primitive_scene_buffer->getRHI())
                        .setSlot(1)
                        .setOffset(source.instance.instance_offset));
            }
            graphics_state.setIndexBuffer(cmd.getIndexBinding());
            command_list.setGraphicsState(graphics_state);
            command_list.drawIndexed(cmd.getDrawArguments());
        }
    }

} // namespace dodoe
