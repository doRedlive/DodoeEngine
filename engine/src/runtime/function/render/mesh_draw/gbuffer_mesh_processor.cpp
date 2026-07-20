// do@Redlive

#include "gbuffer_mesh_processor.h"

#include "mesh_draw_types.h"
#include "cached_mesh_draw_command.h"
#include "runtime/core/math/math.h"
#include "../render_scene/primitive_render_object.h"
#include "runtime/function/graphics/gfx_context.h"

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

        UInt32 resolveTextureIndex(TextureManager* texture_manager, const Ref<Material>& material) {
            if (!texture_manager) {
                return 0;
            }

            UInt32 texture_index = 0;
            auto fallback_texture = texture_manager->getFallback();
            if (fallback_texture && fallback_texture->getDescriptorIndex() >= 0) {
                texture_index = static_cast<UInt32>(fallback_texture->getDescriptorIndex());
            }

            if (material && material->base_color_texture.isValid()) {
                auto* texture = static_cast<Texture2D*>(texture_manager->findTexture(static_cast<InstanceID>(material->base_color_texture.getID())));
                if (texture && texture->getDescriptorIndex() >= 0) {
                    texture_index = static_cast<UInt32>(texture->getDescriptorIndex());
                }
            }

            return texture_index;
        }

        UInt32 resolveMetallicRoughnessTextureIndex(TextureManager* texture_manager, const Ref<Material>& material) {
            if (!texture_manager || !material || !material->metallic_roughness_texture.isValid()) {
                return 0;
            }

            auto* texture = static_cast<Texture2D*>(texture_manager->findTexture(static_cast<InstanceID>(material->metallic_roughness_texture.getID())));
            if (texture && texture->getDescriptorIndex() >= 0) {
                return static_cast<UInt32>(texture->getDescriptorIndex());
            }
            return 0;
        }

        GfxBindingSetHandle descriptorTableBindingSet(DescriptorTableManager* descriptor_table) {
            if (!descriptor_table) {
                return nullptr;
            }
            auto* table = descriptor_table->getDescriptorTable();
            if (!table) {
                return nullptr;
            }
            return create_ref<GfxBindingSet>(cutie::BindingSetHandle(table));
        }

    } // namespace

    void GBufferMeshProcessor::initialize(GfxContext& gfx_context, DescriptorTableManager* descriptor_table, TextureManager* texture_manager) {
        m_descriptor_table = descriptor_table;
        m_texture_manager = texture_manager;

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
        m_texture_manager = nullptr;
        m_descriptor_table = nullptr;
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

        const auto frustum_planes = extractFrustumPlanes(view_projection);

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

                GBufferMeshDrawShaderData draw_shader_data{};
                draw_shader_data.view_projection = view_projection;
                draw_shader_data.time_data = Vector4f(0.0f);
                const Ref<Material>& material = batch.material;
                UInt32 base_color_tex_idx = 0;
                UInt32 metallic_roughness_tex_idx = 0;
                if (material) {
                    base_color_tex_idx = resolveTextureIndex(m_texture_manager, material);
                    metallic_roughness_tex_idx = resolveMetallicRoughnessTextureIndex(m_texture_manager, material);
                    draw_shader_data.draw_data.x = static_cast<Int32>(base_color_tex_idx);
                    draw_shader_data.draw_data.y = static_cast<Int32>(metallic_roughness_tex_idx);
                    draw_shader_data.draw_data.z = material->metallic_roughness_texture.isValid() ? 1 : 0;
                    draw_shader_data.material_data.x = Math::Clamp(material->metallic, 0.0f, 1.0f);
                    draw_shader_data.material_data.y = Math::Clamp(material->roughness, 0.04f, 1.0f);
                }

                const UInt32 shader_data_index = static_cast<UInt32>(out_shader_data.size());
                out_shader_data.push_back(draw_shader_data);

                const auto cache_key = CacheHashUtils::MakeCacheKey(
                    element, material, base_color_tex_idx, metallic_roughness_tex_idx, MeshPassType::GBuffer);

                MeshDrawCommand cached_cmd{};
                cached_cmd.pass_type = MeshPassType::GBuffer;
                cached_cmd.binding_sets.push_back(m_binding_set);
                const auto descriptor_binding_set = descriptorTableBindingSet(m_descriptor_table);
                if (descriptor_binding_set) {
                    cached_cmd.binding_sets.push_back(descriptor_binding_set);
                }
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

        const auto frustum_planes = extractFrustumPlanes(view_projection);

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

                GBufferMeshDrawShaderData draw_shader_data{};
                draw_shader_data.view_projection = view_projection;
                draw_shader_data.time_data = Vector4f(0.0f);
                const Ref<Material>& material = batch.material;
                UInt32 base_color_tex_idx = 0;
                UInt32 metallic_roughness_tex_idx = 0;
                if (material) {
                    base_color_tex_idx = resolveTextureIndex(m_texture_manager, material);
                    metallic_roughness_tex_idx = resolveMetallicRoughnessTextureIndex(m_texture_manager, material);
                    draw_shader_data.draw_data.x = static_cast<Int32>(base_color_tex_idx);
                    draw_shader_data.draw_data.y = static_cast<Int32>(metallic_roughness_tex_idx);
                    draw_shader_data.draw_data.z = material->metallic_roughness_texture.isValid() ? 1 : 0;
                    draw_shader_data.material_data.x = Math::Clamp(material->metallic, 0.0f, 1.0f);
                    draw_shader_data.material_data.y = Math::Clamp(material->roughness, 0.04f, 1.0f);
                }

                const UInt32 shader_data_index = static_cast<UInt32>(out_shader_data.size());
                out_shader_data.push_back(draw_shader_data);

                MeshDrawCommand cmd{};
                cmd.pass_type = MeshPassType::GBuffer;
                cmd.binding_sets.push_back(m_binding_set);
                const auto descriptor_binding_set = descriptorTableBindingSet(m_descriptor_table);
                if (descriptor_binding_set) {
                    cmd.binding_sets.push_back(descriptor_binding_set);
                }
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

} // dodoe
