// do@Redlive

#include "gbuffer_mesh_processor.h"

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
                auto texture = texture_manager->findTexture(static_cast<InstanceID>(material->base_color_texture.getID()));
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

            auto texture = texture_manager->findTexture(static_cast<InstanceID>(material->metallic_roughness_texture.getID()));
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
            return GfxBindingSetHandle(table);
        }

    } // namespace

    void GBufferMeshProcessor::initialize(GfxContext& gfx_context, DescriptorTableManager* descriptor_table, TextureManager* texture_manager) {
        m_descriptor_table = descriptor_table;
        m_texture_manager = texture_manager;

        const auto device = gfx_context.getDevice();
        DO_ASSERT(device != nullptr, "GBufferMeshProcessor device is null");

        m_sampler = device->createSampler(SamplerDesc());
        m_binding_layout = device->createBindingLayout(
            BindingLayoutDesc()
                .setVisibility(ShaderType::All)
                .addItem(BindingLayoutItem::VolatileConstantBuffer(0))
                .addItem(BindingLayoutItem::Sampler(0))
        );
        m_constant_buffer = device->createBuffer(
            BufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(GBufferMeshDrawShaderData)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("GBufferMeshProcessor ConstantBuffer")
        );
        m_binding_set = device->createBindingSet(
            BindingSetDesc()
                .addItem(BindingSetItem::ConstantBuffer(0, m_constant_buffer))
                .addItem(BindingSetItem::Sampler(0, m_sampler)),
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

    void GBufferMeshProcessor::buildCommands(
        const ViewMeshDrawContext& view_context,
        const RenderView& view,
        ViewMeshDrawContext& out_view_context) const
    {
        auto& commands = out_view_context.getMeshPassCommands(MeshPassType::GBuffer);
        auto& shader_data = out_view_context.gbuffer_shader_data;
        const auto& visible_primitives = view_context.visible_primitives;
        const auto& relevant_primitive_indices = view_context.getMeshPassPrimitiveIndices(MeshPassType::GBuffer);
        commands.clear();
        shader_data.clear();
        commands.reserve(relevant_primitive_indices.size());
        shader_data.reserve(relevant_primitive_indices.size());

        const auto descriptor_binding_set = descriptorTableBindingSet(m_descriptor_table);
        const auto frustum_planes = extractFrustumPlanes(view.getViewProjectionMatrix());

        for (const UInt32 primitive_index : relevant_primitive_indices) {
            DO_ASSERT(primitive_index < visible_primitives.size(), "GBufferMeshProcessor primitive index out of range");
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
                if (!batch.isValid() || !batch.isRelevant(MeshPassType::GBuffer) || batch.elements.empty()) {
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

                GBufferMeshDrawShaderData draw_shader_data{};
                draw_shader_data.view_projection = view.getViewProjectionMatrix();
                draw_shader_data.time_data = view_context.frame_time_data;
                const Ref<Material>& material = batch.material;
                if (material) {
                    draw_shader_data.draw_data.x = static_cast<Int32>(resolveTextureIndex(m_texture_manager, material));
                    draw_shader_data.draw_data.y = static_cast<Int32>(resolveMetallicRoughnessTextureIndex(m_texture_manager, material));
                    draw_shader_data.draw_data.z = material->metallic_roughness_texture.isValid() ? 1 : 0;
                    draw_shader_data.material_data.x = glm::clamp(material->metallic, 0.0f, 1.0f);
                    draw_shader_data.material_data.y = glm::clamp(material->roughness, 0.04f, 1.0f);
                }

                const UInt32 shader_data_index = static_cast<UInt32>(shader_data.size());
                shader_data.push_back(draw_shader_data);

                MeshDrawCommand command{};
                command.pass_type = MeshPassType::GBuffer;
                command.primitive_index = static_cast<UInt32>(primitive_index);
                command.shader_data_index = shader_data_index;
                command.binding_sets.push_back(m_binding_set);
                if (descriptor_binding_set) {
                    command.binding_sets.push_back(descriptor_binding_set);
                }
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
