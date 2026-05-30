// do@Redlive

#include "mesh_pass_processor.h"

#include "../render_scene.h"
#include "runtime/core/utils/common.h"

namespace dodoe {

    Bool MeshPassProcessor::initialize(RhiContext* rhi, const MeshPipelineStateDesc& desc) {
        m_rhi = rhi;

        auto state = create_scope<MeshPipelineState>();
        state->m_desc = desc;

        auto device = m_rhi->getDevice();

        auto vert_source = ReadShaderFile(desc.vertex_shader_path);
        auto frag_source = ReadShaderFile(desc.pixel_shader_path);
        state->m_vertex_shader = device->createShader(
            rhi::ShaderDesc().setShaderType(rhi::ShaderType::Vertex)
                .setEntryName("main").setDebugName((desc.debug_name + " VS").c_str()),
            vert_source.data(), vert_source.size());
        state->m_pixel_shader = device->createShader(
            rhi::ShaderDesc().setShaderType(rhi::ShaderType::Pixel)
                .setEntryName("main").setDebugName((desc.debug_name + " PS").c_str()),
            frag_source.data(), frag_source.size());

        if (!desc.geometry_shader_path.empty()) {
            auto geom_source = ReadShaderFile(desc.geometry_shader_path);
            state->m_geometry_shader = device->createShader(
                rhi::ShaderDesc().setShaderType(rhi::ShaderType::Geometry)
                    .setEntryName("main").setDebugName((desc.debug_name + " GS").c_str()),
                geom_source.data(), geom_source.size());
        }

        state->m_input_layout = createStandardInputLayout(state->m_vertex_shader, desc);

        auto binding_layout_desc = rhi::BindingLayoutDesc()
            .setVisibility(rhi::ShaderType::All)
            .addItem(rhi::BindingLayoutItem::VolatileConstantBuffer(0));
        for (const auto& item : desc.extra_binding_items) {
            binding_layout_desc.addItem(item);
        }
        state->m_binding_layout = device->createBindingLayout(binding_layout_desc);

        auto buffer_desc = rhi::BufferDesc()
            .setByteSize(static_cast<UInt32>(desc.constant_buffer_size))
            .setIsConstantBuffer(true)
            .setIsVolatile(true)
            .setMaxVersions(desc.constant_buffer_max_versions)
            .setDebugName((desc.debug_name + " ConstantBuffer").c_str());
        state->m_constant_buffer = device->createBuffer(buffer_desc);

        auto binding_set_desc = rhi::BindingSetDesc()
            .addItem(rhi::BindingSetItem::ConstantBuffer(0, state->m_constant_buffer));
        for (const auto& item : desc.extra_binding_set_items) {
            binding_set_desc.addItem(item);
        }
        state->m_binding_set = device->createBindingSet(binding_set_desc, state->m_binding_layout);

        m_pipeline_state = std::move(state);
        return true;
    }

    void MeshPassProcessor::shutdown() {
        m_command_cache.clear();
        m_pipeline_state = nullptr;
        m_rhi = nullptr;
    }

    void MeshPassProcessor::createGraphicsPipeline(rhi::FramebufferHandle framebuffer) {
        if (!m_pipeline_state) {
            return;
        }
        if (m_pipeline_state->m_pipeline) {
            return;
        }
        if (!framebuffer || !m_pipeline_state->m_vertex_shader ||
            !m_pipeline_state->m_pixel_shader || !m_pipeline_state->m_binding_layout) {
            return;
        }

        auto framebuffer_info = framebuffer->getFramebufferInfo();

        auto pipeline_desc = rhi::GraphicsPipelineDesc()
            .setVertexShader(m_pipeline_state->m_vertex_shader)
            .setPixelShader(m_pipeline_state->m_pixel_shader)
            .addBindingLayout(m_pipeline_state->m_binding_layout)
            .setPrimType(m_pipeline_state->m_desc.primitive_type)
            .setRenderState(m_pipeline_state->m_desc.render_state);

        if (m_pipeline_state->m_input_layout) {
            pipeline_desc.setInputLayout(m_pipeline_state->m_input_layout);
        }

        if (m_pipeline_state->m_desc.descriptor_table) {
            auto* table = m_pipeline_state->m_desc.descriptor_table->getDescriptorTable();
            if (table) {
                pipeline_desc.addBindingLayout(table->getLayout());
            }
        }

        if (m_pipeline_state->m_geometry_shader) {
            pipeline_desc.setGeometryShader(m_pipeline_state->m_geometry_shader);
        }

        m_pipeline_state->m_pipeline = m_rhi->getDevice()->createGraphicsPipeline(
            pipeline_desc, framebuffer_info);
    }

    DynamicArray<MeshBatch> MeshPassProcessor::buildMeshBatches(
        const DynamicArray<Ref<MeshInstance>>& visible_instances) const
    {
        DynamicArray<MeshBatch> batches;

        for (Size_t begin = 0; begin < visible_instances.size();) {
            const auto& first = visible_instances[begin];
            if (!first || !first->getMesh()) {
                ++begin;
                continue;
            }

            const auto& mesh = first->getMesh();
            Size_t end = begin + 1;
            while (end < visible_instances.size()) {
                const auto& instance = visible_instances[end];
                if (!instance || instance->getMesh() != mesh) {
                    break;
                }
                ++end;
            }

            if (!mesh->buffers || !mesh->buffers->vertex_buffer ||
                !mesh->buffers->index_buffer || !mesh->buffers->instance_buffer) {
                begin = end;
                continue;
            }

            const auto& buffers = mesh->buffers;
            auto instance_count = static_cast<UInt32>(end - begin);

            for (const auto& geometry : mesh->geometries) {
                if (!geometry || geometry->index_count == 0) {
                    continue;
                }

                MeshBatchElement element;
                element.index_count = geometry->index_count;
                element.index_offset = geometry->index_offset;
                element.vertex_offset = geometry->vertex_offset;
                element.first_instance = 0;
                element.num_instances = instance_count;
                element.vertex_buffers[0] = buffers->vertex_buffer;
                element.vertex_buffers[1] = buffers->instance_buffer;
                element.vertex_buffers[2] = buffers->instance_id_buffer;
                element.index_buffer = buffers->index_buffer;

                MeshBatch batch;
                batch.material = geometry->material;
                batch.elements.push_back(element);
                batches.push_back(batch);
            }

            begin = end;
        }

        return batches;
    }

    DynamicArray<MeshDrawCommand> MeshPassProcessor::buildDrawCommands(
        const DynamicArray<MeshBatch>& batches,
        rhi::CommandListHandle cmd_list,
        const PerBatchConstantsFn& per_batch_fn)
    {
        DynamicArray<MeshDrawCommand> commands;
        commands.reserve(batches.size());

        if (!m_pipeline_state) {
            return commands;
        }

        const Size_t pass_hash = reinterpret_cast<Size_t>(m_pipeline_state.get());

        for (const auto& batch : batches) {
            if (!batch.isValid()) {
                continue;
            }

            const auto& element = batch.elements[0];

            if (!m_pipeline_state->m_desc.disable_caching) {
                MeshDrawCommandCacheKey cache_key;
                cache_key.batch_hash = reinterpret_cast<Size_t>(element.vertex_buffers[0].Get());
                cache_key.material_hash = batch.material
                    ? reinterpret_cast<Size_t>(batch.material.get()) : 0;
                cache_key.pass_hash = pass_hash;

                auto cache_it = m_command_cache.find(cache_key);
                if (cache_it != m_command_cache.end()) {
                    if (per_batch_fn) {
                        per_batch_fn(cmd_list, batch, m_pipeline_state->m_constant_buffer);
                    }
                    cache_it->second.draw_args.instanceCount = element.num_instances;
                    cache_it->second.draw_args.vertexCount = element.index_count;
                    commands.push_back(cache_it->second);
                    continue;
                }
            }

            MeshDrawCommand command;
            command.pipeline = m_pipeline_state->m_pipeline;

            command.binding_sets.push_back(m_pipeline_state->m_binding_set);
            if (m_pipeline_state->m_desc.descriptor_table) {
                auto* table = m_pipeline_state->m_desc.descriptor_table->getDescriptorTable();
                if (table) {
                    command.binding_sets.push_back(
                        rhi::BindingSetHandle(table));
                }
            }

            for (UInt32 slot = 0; slot < MeshBatchElement::kMaxVertexBufferSlots; ++slot) {
                if (element.vertex_buffers[slot]) {
                    command.vertex_bindings.push_back(
                        rhi::VertexBufferBinding()
                            .setBuffer(element.vertex_buffers[slot])
                            .setSlot(slot).setOffset(0));
                }
            }

            command.index_binding = rhi::IndexBufferBinding()
                .setBuffer(element.index_buffer)
                .setFormat(rhi::Format::R32_UINT)
                .setOffset(0);

            command.draw_args = rhi::DrawArguments()
                .setVertexCount(element.index_count)
                .setInstanceCount(element.num_instances)
                .setStartIndexLocation(element.index_offset)
                .setStartVertexLocation(element.vertex_offset);

            command.sort_key = reinterpret_cast<UInt64>(command.pipeline.Get());

            if (per_batch_fn) {
                per_batch_fn(cmd_list, batch, m_pipeline_state->m_constant_buffer);
            }

            if (!m_pipeline_state->m_desc.disable_caching) {
                MeshDrawCommandCacheKey cache_key;
                cache_key.batch_hash = reinterpret_cast<Size_t>(element.vertex_buffers[0].Get());
                cache_key.material_hash = batch.material
                    ? reinterpret_cast<Size_t>(batch.material.get()) : 0;
                cache_key.pass_hash = pass_hash;
                m_command_cache[cache_key] = command;
            }
            commands.push_back(command);
        }

        std::sort(commands.begin(), commands.end(),
            [](const MeshDrawCommand& a, const MeshDrawCommand& b) {
                return a.sort_key < b.sort_key;
            });

        return commands;
    }

    void MeshPassProcessor::submitDrawCommands(
        const DynamicArray<MeshDrawCommand>& commands,
        rhi::FramebufferHandle framebuffer,
        const Vector2i& viewport_extent,
        rhi::CommandListHandle cmd_list) const
    {
        if (commands.empty()) {
            return;
        }

        auto viewport_state = rhi::ViewportState().addViewportAndScissorRect(
            rhi::Viewport(static_cast<float>(viewport_extent.x),
                          static_cast<float>(viewport_extent.y)));

        rhi::GraphicsPipelineHandle current_pipeline = nullptr;

        for (const auto& cmd : commands) {
            auto graphics_state = rhi::GraphicsState()
                .setFramebuffer(framebuffer)
                .setViewport(viewport_state);

            if (cmd.pipeline != current_pipeline) {
                graphics_state.setPipeline(cmd.pipeline);
                current_pipeline = cmd.pipeline;
            }

            for (const auto& bs : cmd.binding_sets) {
                if (bs) {
                    graphics_state.addBindingSet(bs);
                }
            }

            for (const auto& vb : cmd.vertex_bindings) {
                graphics_state.addVertexBuffer(vb);
            }

            graphics_state.setIndexBuffer(cmd.index_binding);

            cmd_list->setGraphicsState(graphics_state);
            cmd_list->drawIndexed(cmd.draw_args);
        }
    }

    void MeshPassProcessor::invalidatePipeline() {
        if (m_pipeline_state) {
            m_pipeline_state->m_pipeline = nullptr;
        }
    }

    void MeshPassProcessor::invalidateCache() {
        m_command_cache.clear();
    }

    rhi::BufferHandle MeshPassProcessor::getConstantBuffer() const {
        if (m_pipeline_state) {
            return m_pipeline_state->m_constant_buffer;
        }
        return nullptr;
    }

    rhi::InputLayoutHandle MeshPassProcessor::createStandardInputLayout(
        rhi::ShaderHandle vertex_shader,
        const MeshPipelineStateDesc& desc)
    {
        if (!desc.vertex_attributes.empty()) {
            return m_rhi->getDevice()->createInputLayout(
                desc.vertex_attributes.data(),
                static_cast<UInt32>(desc.vertex_attributes.size()),
                vertex_shader);
        }

        auto cache_it = s_input_layout_cache.find(vertex_shader);
        if (cache_it != s_input_layout_cache.end() && desc.extra_vertex_attributes.empty()) {
            return cache_it->second;
        }

        constexpr Size_t kVertexStride   = sizeof(Vector3f) + sizeof(UInt32) + sizeof(Vector2f);
        constexpr Size_t kPositionOff    = 0;
        constexpr Size_t kNormalOff      = sizeof(Vector3f);
        constexpr Size_t kTexCoordOff    = sizeof(Vector3f) + sizeof(UInt32);
        constexpr Size_t kInstanceStride = sizeof(Matrix4f);

        DynamicArray<rhi::VertexAttributeDesc> attributes = {
            rhi::VertexAttributeDesc()
                .setName("a_Position").setFormat(rhi::Format::RGB32_FLOAT)
                .setOffset(kPositionOff).setElementStride(kVertexStride),
            rhi::VertexAttributeDesc()
                .setName("a_Normal").setFormat(rhi::Format::RGBA8_SNORM)
                .setOffset(kNormalOff).setElementStride(kVertexStride),
            rhi::VertexAttributeDesc()
                .setName("a_UV").setFormat(rhi::Format::RG32_FLOAT)
                .setOffset(kTexCoordOff).setElementStride(kVertexStride),
            rhi::VertexAttributeDesc()
                .setName("a_Model0").setFormat(rhi::Format::RGBA32_FLOAT)
                .setBufferIndex(1).setOffset(0)
                .setElementStride(kInstanceStride).setIsInstanced(true),
            rhi::VertexAttributeDesc()
                .setName("a_Model1").setFormat(rhi::Format::RGBA32_FLOAT)
                .setBufferIndex(1).setOffset(sizeof(Vector4f))
                .setElementStride(kInstanceStride).setIsInstanced(true),
            rhi::VertexAttributeDesc()
                .setName("a_Model2").setFormat(rhi::Format::RGBA32_FLOAT)
                .setBufferIndex(1).setOffset(sizeof(Vector4f) * 2)
                .setElementStride(kInstanceStride).setIsInstanced(true),
            rhi::VertexAttributeDesc()
                .setName("a_Model3").setFormat(rhi::Format::RGBA32_FLOAT)
                .setBufferIndex(1).setOffset(sizeof(Vector4f) * 3)
                .setElementStride(kInstanceStride).setIsInstanced(true),
        };

        for (const auto& attr : desc.extra_vertex_attributes) {
            attributes.push_back(attr);
        }

        auto input_layout = m_rhi->getDevice()->createInputLayout(
            attributes.data(),
            static_cast<UInt32>(attributes.size()),
            vertex_shader);

        if (desc.extra_vertex_attributes.empty()) {
            s_input_layout_cache[vertex_shader] = input_layout;
        }

        return input_layout;
    }

} // dodoe
