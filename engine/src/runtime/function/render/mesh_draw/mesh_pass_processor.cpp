// do@Redlive

#include "mesh_pass_processor.h"

#include "../render_scene/render_scene.h"
#include "runtime/core/utils/common.h"

namespace dodoe {

    Bool MeshPassProcessor::initialize(GfxContext* gfx, const MeshPipelineStateDesc& desc) {
        m_gfx = gfx;

        auto state = create_scope<MeshPipelineState>();
        state->m_desc = desc;

        auto device = m_gfx->getDevice();

        auto vert_source = ReadShaderFile(desc.vertex_shader_path);
        auto frag_source = ReadShaderFile(desc.pixel_shader_path);
        state->m_vertex_shader = device->createShader(
            GfxShaderDesc().setShaderType(GfxShaderType::Vertex)
                .setEntryName("main").setDebugName((desc.debug_name + " VS").c_str()),
            vert_source.data(), vert_source.size());
        state->m_pixel_shader = device->createShader(
            GfxShaderDesc().setShaderType(GfxShaderType::Pixel)
                .setEntryName("main").setDebugName((desc.debug_name + " PS").c_str()),
            frag_source.data(), frag_source.size());

        if (!desc.geometry_shader_path.empty()) {
            auto geom_source = ReadShaderFile(desc.geometry_shader_path);
            state->m_geometry_shader = device->createShader(
                GfxShaderDesc().setShaderType(GfxShaderType::Geometry)
                    .setEntryName("main").setDebugName((desc.debug_name + " GS").c_str()),
                geom_source.data(), geom_source.size());
        }

        state->m_input_layout = createStandardInputLayout(state->m_vertex_shader, desc);

        auto binding_layout_desc = GfxBindingLayoutDesc()
            .setVisibility(GfxShaderType::All)
            .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(0));
        for (const auto& item : desc.extra_binding_items) {
            binding_layout_desc.addItem(item);
        }
        state->m_binding_layout = device->createBindingLayout(binding_layout_desc);

        auto buffer_desc = GfxBufferDesc()
            .setByteSize(static_cast<UInt32>(desc.constant_buffer_size))
            .setIsConstantBuffer(true)
            .setIsVolatile(true)
            .setMaxVersions(desc.constant_buffer_max_versions)
            .setDebugName((desc.debug_name + " ConstantBuffer").c_str());
        state->m_constant_buffer = device->createBuffer(buffer_desc);

        auto binding_set_desc = GfxBindingSetDesc()
            .addItem(GfxBindingSetItem::ConstantBuffer(0, state->m_constant_buffer));
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
        m_gfx = nullptr;
    }

    void MeshPassProcessor::createGraphicsPipeline(GfxFramebufferHandle framebuffer) {
        DO_DEBUG("MeshPassProcessor::createGraphicsPipeline called");
        if (!m_pipeline_state) {
            DO_DEBUG("MeshPassProcessor::createGraphicsPipeline: no pipeline_state");
            return;
        }
        if (m_pipeline_state->m_pipeline) {
            DO_DEBUG("MeshPassProcessor::createGraphicsPipeline: pipeline already exists");
            return;
        }
        if (!framebuffer || !m_pipeline_state->m_vertex_shader ||
            !m_pipeline_state->m_pixel_shader || !m_pipeline_state->m_binding_layout) {
            DO_DEBUG("MeshPassProcessor::createGraphicsPipeline: missing resources (framebuffer={} vs={} ps={} layout={})",
                framebuffer != nullptr,
                m_pipeline_state->m_vertex_shader != nullptr,
                m_pipeline_state->m_pixel_shader != nullptr,
                m_pipeline_state->m_binding_layout != nullptr);
            return;
        }

        auto framebuffer_info = framebuffer->getFramebufferInfo();

        auto pipeline_desc = GfxGraphicsPipelineDesc()
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

        DO_DEBUG("MeshPassProcessor::createGraphicsPipeline: calling device->createGraphicsPipeline");
        m_pipeline_state->m_pipeline = m_gfx->getDevice()->createGraphicsPipeline(
            pipeline_desc, framebuffer_info);
        DO_DEBUG("MeshPassProcessor::createGraphicsPipeline: pipeline created, handle={}", m_pipeline_state->m_pipeline != nullptr);
    }

    DynamicArray<MeshBatch> MeshPassProcessor::buildMeshBatches(
        const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
        const MeshPassType pass_type) const
    {
        DynamicArray<MeshBatch> batches;

        for (Size_t primitive_index = 0; primitive_index < visible_primitives.size(); primitive_index++) {
            const auto* primitive = visible_primitives[primitive_index];
            if (!primitive) {
                continue;
            }

            const auto& primitive_batches = primitive->getMeshBatches();
            batches.reserve(batches.size() + primitive_batches.size());
            for (const auto& batch : primitive_batches) {
                if (batch.isValid() && batch.isRelevant(pass_type)) {
                    batches.push_back(batch);
                }
            }
        }

        return batches;
    }

    DynamicArray<MeshDrawCommand> MeshPassProcessor::buildDrawCommands(
        const DynamicArray<MeshBatch>& batches,
        GfxCommandListHandle cmd_list,
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
                cache_key.batch_hash =
                    reinterpret_cast<Size_t>(element.vertex_buffer.Get()) ^
                    (static_cast<Size_t>(batch.primitive_id) << 1) ^
                    (static_cast<Size_t>(element.section_index) << 9);
                cache_key.material_hash = batch.material
                    ? reinterpret_cast<Size_t>(batch.material.get()) : 0;
                cache_key.pass_hash = pass_hash;

                auto cache_it = m_command_cache.find(cache_key);
                if (cache_it != m_command_cache.end()) {
                    if (per_batch_fn) {
                        per_batch_fn(cmd_list, batch, m_pipeline_state->m_constant_buffer);
                    }
                    cache_it->second.draw_args.instanceCount = 1;
                    cache_it->second.draw_args.vertexCount = element.index_count;
                    cache_it->second.draw_args.startIndexLocation = element.index_offset;
                    cache_it->second.draw_args.startVertexLocation = element.vertex_offset;
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
                        GfxBindingSetHandle(table));
                }
            }

            command.vertex_bindings.push_back(
                GfxVertexBufferBinding()
                    .setBuffer(element.vertex_buffer)
                    .setSlot(0).setOffset(0));

            command.index_binding = GfxIndexBufferBinding()
                .setBuffer(element.index_buffer)
                .setFormat(GfxFormat::R32_UINT)
                .setOffset(0);

            command.draw_args = GfxDrawArguments()
                .setVertexCount(element.index_count)
                .setInstanceCount(1)
                .setStartIndexLocation(element.index_offset)
                .setStartVertexLocation(element.vertex_offset);

            command.sort_key = reinterpret_cast<UInt64>(command.pipeline.Get());

            if (per_batch_fn) {
                per_batch_fn(cmd_list, batch, m_pipeline_state->m_constant_buffer);
            }

            if (!m_pipeline_state->m_desc.disable_caching) {
                MeshDrawCommandCacheKey cache_key;
                cache_key.batch_hash =
                    reinterpret_cast<Size_t>(element.vertex_buffer.Get()) ^
                    (static_cast<Size_t>(batch.primitive_id) << 1) ^
                    (static_cast<Size_t>(element.section_index) << 9);
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
        GfxFramebufferHandle framebuffer,
        const Vector2i& viewport_extent,
        GfxCommandListHandle cmd_list) const
    {
        if (commands.empty()) {
            return;
        }

        auto viewport_state = GfxViewportState().addViewportAndScissorRect(
            GfxViewport(static_cast<float>(viewport_extent.x),
                          static_cast<float>(viewport_extent.y)));

        GfxGraphicsPipelineHandle current_pipeline = nullptr;

        for (const auto& cmd : commands) {
            auto graphics_state = GfxGraphicsState()
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

    GfxBufferHandle MeshPassProcessor::getConstantBuffer() const {
        if (m_pipeline_state) {
            return m_pipeline_state->m_constant_buffer;
        }
        return nullptr;
    }

    GfxInputLayoutHandle MeshPassProcessor::createStandardInputLayout(
        GfxShaderHandle vertex_shader,
        const MeshPipelineStateDesc& desc)
    {
        if (!desc.vertex_attributes.empty()) {
            return m_gfx->getDevice()->createInputLayout(
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

        DynamicArray<GfxVertexAttributeDesc> attributes = {
            GfxVertexAttributeDesc()
                .setName("a_Position").setFormat(GfxFormat::RGB32_FLOAT)
                .setOffset(kPositionOff).setElementStride(kVertexStride),
            GfxVertexAttributeDesc()
                .setName("a_Normal").setFormat(GfxFormat::RGBA8_SNORM)
                .setOffset(kNormalOff).setElementStride(kVertexStride),
            GfxVertexAttributeDesc()
                .setName("a_UV").setFormat(GfxFormat::RG32_FLOAT)
                .setOffset(kTexCoordOff).setElementStride(kVertexStride),
            GfxVertexAttributeDesc()
                .setName("a_Model0").setFormat(GfxFormat::RGBA32_FLOAT)
                .setBufferIndex(1).setOffset(0)
                .setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc()
                .setName("a_Model1").setFormat(GfxFormat::RGBA32_FLOAT)
                .setBufferIndex(1).setOffset(sizeof(Vector4f))
                .setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc()
                .setName("a_Model2").setFormat(GfxFormat::RGBA32_FLOAT)
                .setBufferIndex(1).setOffset(sizeof(Vector4f) * 2)
                .setElementStride(kInstanceStride).setIsInstanced(true),
            GfxVertexAttributeDesc()
                .setName("a_Model3").setFormat(GfxFormat::RGBA32_FLOAT)
                .setBufferIndex(1).setOffset(sizeof(Vector4f) * 3)
                .setElementStride(kInstanceStride).setIsInstanced(true),
        };

        for (const auto& attr : desc.extra_vertex_attributes) {
            attributes.push_back(attr);
        }

        auto input_layout = m_gfx->getDevice()->createInputLayout(
            attributes.data(),
            static_cast<UInt32>(attributes.size()),
            vertex_shader);

        if (desc.extra_vertex_attributes.empty()) {
            s_input_layout_cache[vertex_shader] = input_layout;
        }

        return input_layout;
    }

} // dodoe
