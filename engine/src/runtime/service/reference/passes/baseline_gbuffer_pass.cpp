// do@Redlive

#include "baseline_gbuffer_pass.h"

#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"
#include "runtime/function/render/mesh_draw/mesh_batch.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/function/render/render_frame/frame_telemetry.h"
#include "runtime/function/render/material/material_system.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/shader/descriptor_table_manager.h"
#include "runtime/core/math/math.h"
#include "runtime/service/debug/debug_imgui.h"

namespace dodoe {

    namespace {
        constexpr UInt32 kInitialInstanceCapacity = 256;
        constexpr UInt32 kVolatileConstantBufferVersions = 4096;

        constexpr Size_t kMeshVertexStride = sizeof(Vector3f) + sizeof(UInt32) + sizeof(Vector2f);
        constexpr Size_t kMeshInstanceStride = sizeof(InstanceSceneData);

        DynamicArray<GfxVertexAttributeDesc> BuildMeshVertexAttributes() {
            return {
                GfxVertexAttributeDesc().setName("a_Position").setFormat(GfxFormat::RGB32_FLOAT).setOffset(0).setElementStride(kMeshVertexStride),
                GfxVertexAttributeDesc().setName("a_Normal").setFormat(GfxFormat::RGBA8_SNORM).setOffset(sizeof(Vector3f)).setElementStride(kMeshVertexStride),
                GfxVertexAttributeDesc().setName("a_UV").setFormat(GfxFormat::RG32_FLOAT).setOffset(sizeof(Vector3f) + sizeof(UInt32)).setElementStride(kMeshVertexStride),
                GfxVertexAttributeDesc().setName("TEXCOORD3").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(0).setElementStride(kMeshInstanceStride).setIsInstanced(true),
                GfxVertexAttributeDesc().setName("TEXCOORD4").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f)).setElementStride(kMeshInstanceStride).setIsInstanced(true),
                GfxVertexAttributeDesc().setName("TEXCOORD5").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f) * 2).setElementStride(kMeshInstanceStride).setIsInstanced(true),
                GfxVertexAttributeDesc().setName("TEXCOORD6").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f) * 3).setElementStride(kMeshInstanceStride).setIsInstanced(true),
                GfxVertexAttributeDesc().setName("a_InstanceColorTint").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Matrix4f)).setElementStride(kMeshInstanceStride).setIsInstanced(true),
                GfxVertexAttributeDesc().setName("a_InstanceParams").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Matrix4f) + sizeof(Vector4f)).setElementStride(kMeshInstanceStride).setIsInstanced(true),
            };
        }
    }

    Bool BaselineGBufferPass::initialize(const BaselinePassContext& context) {
        m_device = context.device;
        m_command_list = context.command_list;
        m_shader_library = context.shader_library;
        m_shared_render_service = context.shared_render_service;

        if (!m_shared_render_service || !m_shader_library) {
            return true;
        }
        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        auto* input_layout_cache = m_shared_render_service->getInputLayoutCache();
        DO_ASSERT(binding_layout_cache != nullptr, "BaselineGBufferPass: binding layout cache is unavailable");
        DO_ASSERT(input_layout_cache != nullptr, "BaselineGBufferPass: input layout cache is unavailable");

        m_global_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Global))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kGlobalBindingConstants)));
        m_view_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::View))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kViewBindingConstants)));
        m_primitive_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Primitive))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kPrimitiveBindingConstants)));
        if (RenderSettings::IsBindlessActive()) {
            m_material_binding_layout = binding_layout_cache->getOrCreate(
                GfxBindingLayoutDesc()
                    .setVisibility(GfxShaderType::All)
                    .setRegisterSpaceIsDescriptorSet(true)
                    .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Material))
                    .addItem(GfxBindingLayoutItem::Sampler(shader_bindings::kMaterialBindingSampler)));
            m_material_binding_set = create_ref<GfxBindingSet>(
                m_device->createBindingSet(
                    GfxBindingSetDesc().addItem(
                        GfxBindingSetItem::Sampler(shader_bindings::kMaterialBindingSampler, context.sampler.Get())),
                    m_material_binding_layout.Get()));
            auto* descriptor_table = m_shared_render_service->getDescriptorTable();
            if (descriptor_table && descriptor_table->getDescriptorTable()) {
                m_bindless_binding_set = create_ref<GfxBindingSet>(
                    cutie::BindingSetHandle(descriptor_table->getDescriptorTable()));
                m_bindless_binding_layout = descriptor_table->getDescriptorTable()->getLayout();
            }
        } else {
            m_material_binding_layout = binding_layout_cache->getOrCreate(
                GfxBindingLayoutDesc()
                    .setVisibility(GfxShaderType::Pixel)
                    .setRegisterSpaceIsDescriptorSet(true)
                    .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Material))
                    .addItem(GfxBindingLayoutItem::Sampler(shader_bindings::kMaterialBindingSampler))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(shader_bindings::kMaterialBindingBaseColor))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(shader_bindings::kMaterialBindingMetallicRough)));
        }

        m_input_layout = input_layout_cache->getOrCreate(BuildMeshVertexAttributes(), m_shader_library->getLitVertexShader());

        GfxBufferDesc cb_desc;
        cb_desc.setByteSize(static_cast<UInt32>(sizeof(GlobalMeshShaderData)))
            .setIsConstantBuffer(true)
            .setIsVolatile(true)
            .setMaxVersions(kVolatileConstantBufferVersions)
            .setDebugName("BaselineGBufferGlobalCB");
        m_global_cb = m_device->createBuffer(cb_desc);
        cb_desc.setByteSize(static_cast<UInt32>(sizeof(ViewMeshShaderData)))
            .setDebugName("BaselineGBufferViewCB");
        m_view_cb = m_device->createBuffer(cb_desc);
        cb_desc.setByteSize(static_cast<UInt32>(sizeof(PrimitiveMeshDrawShaderData)))
            .setDebugName("BaselineGBufferPrimitiveCB");
        m_primitive_cb = m_device->createBuffer(cb_desc);

        m_global_binding_set = m_device->createBindingSet(
            GfxBindingSetDesc().addItem(
                GfxBindingSetItem::ConstantBuffer(shader_bindings::kGlobalBindingConstants, m_global_cb.Get())),
            m_global_binding_layout.Get());
        m_view_binding_set = m_device->createBindingSet(
            GfxBindingSetDesc().addItem(
                GfxBindingSetItem::ConstantBuffer(shader_bindings::kViewBindingConstants, m_view_cb.Get())),
            m_view_binding_layout.Get());
        m_primitive_binding_set = m_device->createBindingSet(
            GfxBindingSetDesc().addItem(
                GfxBindingSetItem::ConstantBuffer(shader_bindings::kPrimitiveBindingConstants, m_primitive_cb.Get())),
            m_primitive_binding_layout.Get());
        return true;
    }

    void BaselineGBufferPass::shutdown() {
        m_pipeline = nullptr;
        m_input_layout = nullptr;
        m_global_binding_layout = nullptr;
        m_view_binding_layout = nullptr;
        m_material_binding_layout = nullptr;
        m_primitive_binding_layout = nullptr;
        m_bindless_binding_layout = nullptr;
        m_global_cb = nullptr;
        m_view_cb = nullptr;
        m_primitive_cb = nullptr;
        m_instance_buffer = nullptr;
        m_instance_capacity = 0;
        m_global_binding_set = nullptr;
        m_view_binding_set = nullptr;
        m_primitive_binding_set = nullptr;
        m_material_binding_set = nullptr;
        m_bindless_binding_set = nullptr;
        m_shader_library = nullptr;
        m_shared_render_service = nullptr;
        m_command_list = nullptr;
        m_device = nullptr;
    }

    void BaselineGBufferPass::ensurePipeline(const cutie::FramebufferInfo& framebuffer_info) {
        if (m_pipeline) {
            return;
        }
        if (!m_shader_library || !m_shared_render_service) {
            return;
        }
        const auto vertex_shader = m_shader_library->getLitVertexShader();
        const auto pixel_shader = m_shader_library->getGBufferPixelShader();
        if (!vertex_shader || !pixel_shader) {
            DO_ERROR("BaselineGBufferPass: gbuffer shaders are not loaded");
            return;
        }

        GfxGraphicsPipelineDesc pipeline_desc;
        pipeline_desc.setPrimType(GfxPrimitiveType::TriangleList);
        pipeline_desc.setVertexShader(vertex_shader.Get());
        pipeline_desc.setPixelShader(pixel_shader.Get());
        pipeline_desc.setInputLayout(m_input_layout.Get());
        pipeline_desc.addBindingLayout(m_global_binding_layout.Get());
        pipeline_desc.addBindingLayout(m_view_binding_layout.Get());
        pipeline_desc.addBindingLayout(m_material_binding_layout.Get());
        if (RenderSettings::IsBindlessActive() && m_bindless_binding_layout) {
            pipeline_desc.addBindingLayout(m_bindless_binding_layout);
        }
        pipeline_desc.addBindingLayout(m_primitive_binding_layout.Get());

        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(GfxComparisonFunc::Less).disableStencil();
        GfxRasterState raster_state;
        raster_state.setCullNone();
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state)
            .setRasterState(raster_state);
        pipeline_desc.setRenderState(render_state);

        m_pipeline = m_device->createGraphicsPipeline(pipeline_desc, framebuffer_info);
        DO_INFO("BaselineGBufferPass: render pipeline created");
    }

    void BaselineGBufferPass::ensureInstanceCapacity(UInt32 instance_count) {
        if (instance_count <= m_instance_capacity) {
            return;
        }
        UInt32 capacity = m_instance_capacity > 0 ? m_instance_capacity : kInitialInstanceCapacity;
        while (capacity < instance_count) {
            capacity *= 2;
        }
        GfxBufferDesc desc;
        desc.setByteSize(static_cast<UInt64>(capacity) * sizeof(InstanceSceneData))
            .setIsVertexBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
            .setDebugName("BaselineGBufferInstances");
        m_instance_buffer = m_device->createBuffer(desc);
        m_instance_capacity = capacity;
    }

    void BaselineGBufferPass::setupView(RenderView& view, RenderViewFamily& view_family) {
        auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();
        mesh_ext.frame_time_data = Vector4f(view_family.getTimeSeconds(), view_family.getDeltaSeconds(), 0.0f, 0.0f);

        // Entity selected in the debug ImGui hierarchy panel is highlighted this frame.
        UInt64 selected_uuid = 0;
        Entity selected_entity = DebugImGui::GetSelectedEntity();
        if (selected_entity.valid()) {
            selected_uuid = static_cast<UInt64>(selected_entity.uuid());
        }

        Size_t total_instance_count = 0;
        for (const auto* primitive : mesh_ext.visible_primitives) {
            total_instance_count += primitive ? primitive->getInstanceCount() : 1;
        }
        mesh_ext.instance_scene_data.clear();
        mesh_ext.instance_scene_data.reserve(total_instance_count);
        for (const auto* primitive : mesh_ext.visible_primitives) {
            if (primitive) {
                const Bool is_selected = selected_uuid != 0 &&
                    primitive->getId().value() == selected_uuid;
                for (const auto& inst_data : primitive->getInstanceSceneData()) {
                    mesh_ext.instance_scene_data.push_back(inst_data);
                    if (is_selected) {
                        // Tint alpha is unused by the lit shaders; 0.0 marks "selected"
                        // so the lighting pass can add a highlight on top.
                        mesh_ext.instance_scene_data.back().color_tint.a = 0.0f;
                    }
                }
            } else {
                mesh_ext.instance_scene_data.push_back(InstanceSceneData{});
            }
        }
        mesh_ext.primitive_mesh_pass_relevance.clear();
        mesh_ext.primitive_mesh_pass_relevance.reserve(mesh_ext.visible_primitives.size());
        for (const auto* primitive : mesh_ext.visible_primitives) {
            MeshPassRelevance relevance{};
            if (primitive && primitive->isVisible()) {
                for (UInt32 pass_index = 0; pass_index < static_cast<UInt32>(MeshPassType::Count); ++pass_index) {
                    const auto pass_type = static_cast<MeshPassType>(pass_index);
                    relevance.setRelevant(pass_type, primitive->hasRelevantBatch(pass_type));
                }
            }
            mesh_ext.primitive_mesh_pass_relevance.push_back(relevance);
        }
        mesh_ext.buildMeshPassPrimitiveIndices();
    }

    void BaselineGBufferPass::render(RenderView& view, RenderScene& scene,
                                     const GfxViewportState& viewport_state, cutie::IFramebuffer* framebuffer) {
        if (!m_pipeline) {
            return;
        }
        const auto* mesh_ext = view.getExtension<MeshViewExtension>();
        if (!mesh_ext || mesh_ext->instance_scene_data.empty()) {
            return;
        }
        const auto& primitive_indices = mesh_ext->getMeshPassPrimitiveIndices(MeshPassType::Opaque);
        if (primitive_indices.empty()) {
            return;
        }
        ensureInstanceCapacity(static_cast<UInt32>(mesh_ext->instance_scene_data.size()));

        m_command_list->setBufferState(m_instance_buffer.Get(), cutie::ResourceStates::CopyDest);
        RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
        m_command_list->writeBuffer(m_instance_buffer.Get(), mesh_ext->instance_scene_data.data(),
            mesh_ext->instance_scene_data.size() * sizeof(InstanceSceneData));

        const GlobalMeshShaderData global_data{mesh_ext->frame_time_data};
        m_command_list->writeBuffer(m_global_cb.Get(), &global_data, sizeof(global_data));
        const ViewMeshShaderData view_data{Math::FlipClipSpaceY(view.getViewProjectionMatrix())};
        m_command_list->writeBuffer(m_view_cb.Get(), &view_data, sizeof(view_data));

        m_command_list->setBufferState(m_instance_buffer.Get(), cutie::ResourceStates::VertexBuffer);
        RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();

        DynamicArray<UInt32> instance_prefix(mesh_ext->visible_primitives.size() + 1, 0);
        for (Size_t i = 0; i < mesh_ext->visible_primitives.size(); ++i) {
            const auto* primitive = mesh_ext->visible_primitives[i];
            instance_prefix[i + 1] = instance_prefix[i] + (primitive ? primitive->getInstanceCount() : 0);
        }

        for (const UInt32 primitive_index : primitive_indices) {
            if (primitive_index >= mesh_ext->visible_primitives.size()) {
                continue;
            }
            const auto* primitive = mesh_ext->visible_primitives[primitive_index];
            if (!primitive) {
                continue;
            }
            const UInt64 instance_offset = static_cast<UInt64>(instance_prefix[primitive_index]) * sizeof(InstanceSceneData);

            for (const auto& batch : primitive->getMeshBatches()) {
                if (!batch.isValid() || !batch.isRelevant(MeshPassType::Opaque) || batch.getElements().empty()) {
                    continue;
                }
                const auto& element = batch.getElements()[0];
                if (!element.isValid() || !element.vertex_buffer || !element.index_buffer ||
                    !element.vertex_buffer->isGpuReady() || !element.index_buffer->isGpuReady()) {
                    continue;
                }
                const auto* material_instance = batch.getMaterialInstance();
                if (!material_instance) {
                    continue;
                }
                GfxBindingSetHandle material_binding_set{};
                GfxBindingSetHandle bindless_binding_set{};
                if (RenderSettings::IsBindlessActive()) {
                    material_binding_set = m_material_binding_set;
                    bindless_binding_set = m_bindless_binding_set;
                    if (!material_binding_set || !bindless_binding_set) {
                        if (!m_material_warning_logged) {
                            m_material_warning_logged = true;
                            DO_WARN("BaselineGBufferPass: bindless descriptor table is unavailable, primitive skipped");
                        }
                        continue;
                    }
                } else {
                    auto* material_system = m_shared_render_service ? m_shared_render_service->getMaterialSystem() : nullptr;
                    material_binding_set = material_system
                        ? material_system->getTextureBindingSet(material_instance)
                        : GfxBindingSetHandle{};
                    if (!material_binding_set || !material_binding_set->isGpuReady()) {
                        if (!m_material_warning_logged) {
                            m_material_warning_logged = true;
                            DO_WARN("BaselineGBufferPass: material has no resolvable texture binding set, primitive skipped");
                        }
                        continue;
                    }
                }

                PrimitiveMeshDrawShaderData shader_data{};
                const auto& descriptor_indices = material_instance->texture_descriptor_indices;
                shader_data.draw_data.x = descriptor_indices.empty()
                    ? -1
                    : descriptor_indices[0];
                shader_data.draw_data.y = descriptor_indices.size() > 1
                    ? descriptor_indices[1]
                    : -1;
                shader_data.draw_data.z = descriptor_indices.size() > 1 ? 1 : 0;
                shader_data.draw_data.w = descriptor_indices.size() > 2
                    ? static_cast<Int32>(descriptor_indices[2])
                    : -1;
                shader_data.material_data.x = material_instance->metallic;
                shader_data.material_data.y = material_instance->roughness;
                shader_data.material_data.z = material_instance->ao;
                shader_data.emissive_data = Vector4f(
                    material_instance->emissive.x, material_instance->emissive.y, material_instance->emissive.z,
                    descriptor_indices.size() > 3
                        ? static_cast<Float>(descriptor_indices[3]) + 1.0f : 0.0f);
                m_command_list->writeBuffer(m_primitive_cb.Get(), &shader_data, sizeof(shader_data));

                cutie::GraphicsState graphics_state;
                graphics_state.setPipeline(m_pipeline.Get());
                graphics_state.setFramebuffer(framebuffer);
                graphics_state.setViewport(viewport_state);
                graphics_state.addBindingSet(m_global_binding_set.Get());
                graphics_state.addBindingSet(m_view_binding_set.Get());
                graphics_state.addBindingSet(material_binding_set->getRHI());
                if (bindless_binding_set) {
                    graphics_state.addBindingSet(bindless_binding_set->getRHI());
                }
                graphics_state.addBindingSet(m_primitive_binding_set.Get());
                graphics_state.addVertexBuffer(
                    cutie::VertexBufferBinding().setBuffer(element.vertex_buffer->getRHI()).setSlot(0).setOffset(0));
                graphics_state.addVertexBuffer(
                    cutie::VertexBufferBinding().setBuffer(m_instance_buffer.Get()).setSlot(1).setOffset(instance_offset));
                graphics_state.setIndexBuffer(
                    cutie::IndexBufferBinding()
                        .setBuffer(element.index_buffer->getRHI())
                        .setFormat(GfxFormat::R32_UINT)
                        .setOffset(0));

                m_command_list->setGraphicsState(graphics_state);
                RenderFrameCounters::Self().addDrawCall(element.instance_count);
                m_command_list->drawIndexed(GfxDrawArguments()
                    .setVertexCount(element.index_count)
                    .setInstanceCount(element.instance_count)
                    .setStartIndexLocation(element.index_offset)
                    .setStartVertexLocation(element.vertex_offset));
            }
        }
    }

} // namespace dodoe
