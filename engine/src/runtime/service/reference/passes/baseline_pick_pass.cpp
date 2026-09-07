// do@Redlive

#include "baseline_pick_pass.h"

#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_scene/primitive_scene_info.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"
#include "runtime/function/render/mesh_draw/mesh_batch.h"
#include "runtime/function/render/material/material_system.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    namespace {
        constexpr UInt32 kVolatileConstantBufferVersions = 256;

        constexpr Size_t kMeshVertexStride = sizeof(Vector3f) + sizeof(UInt32) + sizeof(Vector2f);
        constexpr Size_t kMeshInstanceStride = sizeof(InstanceSceneData);

        DynamicArray<GfxVertexAttributeDesc> BuildPickVertexAttributes() {
            return {
                GfxVertexAttributeDesc().setName("a_Position").setFormat(GfxFormat::RGB32_FLOAT).setOffset(0).setElementStride(kMeshVertexStride),
                GfxVertexAttributeDesc().setName("a_Normal").setFormat(GfxFormat::RGBA8_SNORM).setOffset(sizeof(Vector3f)).setElementStride(kMeshVertexStride),
                GfxVertexAttributeDesc().setName("a_UV").setFormat(GfxFormat::RG32_FLOAT).setOffset(sizeof(Vector3f) + sizeof(UInt32)).setElementStride(kMeshVertexStride),
                GfxVertexAttributeDesc().setName("TEXCOORD3").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(0).setElementStride(kMeshInstanceStride).setIsInstanced(true),
                GfxVertexAttributeDesc().setName("TEXCOORD4").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f)).setElementStride(kMeshInstanceStride).setIsInstanced(true),
                GfxVertexAttributeDesc().setName("TEXCOORD5").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f) * 2).setElementStride(kMeshInstanceStride).setIsInstanced(true),
                GfxVertexAttributeDesc().setName("TEXCOORD6").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f) * 3).setElementStride(kMeshInstanceStride).setIsInstanced(true),
            };
        }
    }

    Bool BaselinePickPass::initialize(const BaselinePassContext& context) {
        m_device = context.device;
        m_command_list = context.command_list;
        m_shader_library = context.shader_library;
        m_shared_render_service = context.shared_render_service;

        if (!m_shared_render_service || !m_shader_library) {
            return true;
        }
        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        auto* input_layout_cache = m_shared_render_service->getInputLayoutCache();
        DO_ASSERT(binding_layout_cache != nullptr, "BaselinePickPass: binding layout cache is unavailable");
        DO_ASSERT(input_layout_cache != nullptr, "BaselinePickPass: input layout cache is unavailable");

        m_view_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Vertex)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::View))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kViewBindingConstants)));
        m_primitive_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Vertex)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Primitive))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kPrimitiveBindingConstants)));

        m_input_layout = input_layout_cache->getOrCreate(BuildPickVertexAttributes(), m_shader_library->getPickVertexShader());

        GfxBufferDesc cb_desc;
        cb_desc.setByteSize(static_cast<UInt32>(sizeof(ViewMeshShaderData)))
            .setIsConstantBuffer(true)
            .setIsVolatile(true)
            .setMaxVersions(kVolatileConstantBufferVersions)
            .setDebugName("BaselinePickViewCB");
        m_view_cb = m_device->createBuffer(cb_desc);
        cb_desc.setByteSize(static_cast<UInt32>(sizeof(PrimitiveMeshDrawShaderData)))
            .setDebugName("BaselinePickPrimitiveCB");
        m_primitive_cb = m_device->createBuffer(cb_desc);

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

    void BaselinePickPass::shutdown() {
        m_pipeline = nullptr;
        m_input_layout = nullptr;
        m_view_binding_layout = nullptr;
        m_primitive_binding_layout = nullptr;
        m_view_cb = nullptr;
        m_primitive_cb = nullptr;
        m_view_binding_set = nullptr;
        m_primitive_binding_set = nullptr;
        m_shader_library = nullptr;
        m_shared_render_service = nullptr;
        m_command_list = nullptr;
        m_device = nullptr;
    }

    void BaselinePickPass::ensurePipeline(const cutie::FramebufferInfo& framebuffer_info) {
        if (m_pipeline || !m_shader_library) {
            return;
        }
        const auto vertex_shader = m_shader_library->getPickVertexShader();
        const auto pixel_shader = m_shader_library->getPickPixelShader();
        if (!vertex_shader || !pixel_shader) {
            if (!m_warning_logged) {
                m_warning_logged = true;
                DO_WARN("BaselinePickPass: pick shaders are not loaded");
            }
            return;
        }

        GfxGraphicsPipelineDesc pipeline_desc;
        pipeline_desc.setPrimType(GfxPrimitiveType::TriangleList);
        pipeline_desc.setVertexShader(vertex_shader.Get());
        pipeline_desc.setPixelShader(pixel_shader.Get());
        pipeline_desc.setInputLayout(m_input_layout.Get());
        pipeline_desc.addBindingLayout(m_view_binding_layout.Get());
        pipeline_desc.addBindingLayout(m_primitive_binding_layout.Get());

        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.enableDepthTest().disableDepthWrite().setDepthFunc(GfxComparisonFunc::LessOrEqual).disableStencil();
        GfxRasterState raster_state;
        raster_state.setCullNone();
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state)
            .setRasterState(raster_state);
        pipeline_desc.setRenderState(render_state);

        m_pipeline = m_device->createGraphicsPipeline(pipeline_desc, framebuffer_info);
        DO_INFO("BaselinePickPass: pick pipeline created");
    }

    Bool BaselinePickPass::render(RenderView& view, const GfxViewportState& viewport_state,
                                  cutie::IFramebuffer* framebuffer, cutie::IBuffer* instance_buffer,
                                  DynamicArray<UInt64>& out_pick_ids) {
        if (!m_pipeline || !instance_buffer) {
            return false;
        }
        const auto* mesh_ext = view.getExtension<MeshViewExtension>();
        if (!mesh_ext || mesh_ext->instance_scene_data.empty()) {
            return false;
        }
        const auto& primitive_indices = mesh_ext->getMeshPassPrimitiveIndices(MeshPassType::Opaque);
        if (primitive_indices.empty()) {
            return false;
        }

        const ViewMeshShaderData view_data{Math::FlipClipSpaceY(view.getViewProjectionMatrix())};
        m_command_list->writeBuffer(m_view_cb.Get(), &view_data, sizeof(view_data));

        DynamicArray<UInt32> instance_prefix(mesh_ext->visible_primitives.size() + 1, 0);
        for (Size_t i = 0; i < mesh_ext->visible_primitives.size(); ++i) {
            const auto* primitive = mesh_ext->visible_primitives[i];
            instance_prefix[i + 1] = instance_prefix[i] + (primitive ? primitive->getInstanceCount() : 0);
        }

        UInt32 slot = 0;
        for (const UInt32 primitive_index : primitive_indices) {
            if (primitive_index >= mesh_ext->visible_primitives.size()) {
                continue;
            }
            const auto* primitive = mesh_ext->visible_primitives[primitive_index];
            if (!primitive) {
                continue;
            }
            const UInt64 instance_offset = static_cast<UInt64>(instance_prefix[primitive_index]) * kMeshInstanceStride;

            const MeshBatchElement* picked_element = nullptr;
            for (const auto& batch : primitive->getMeshBatches()) {
                if (!batch.isValid() || !batch.isRelevant(MeshPassType::Opaque) || batch.elements.empty()) {
                    continue;
                }
                const auto& element = batch.elements[0];
                if (!element.isValid() || !element.vertex_buffer || !element.index_buffer ||
                    !element.vertex_buffer->isGpuReady() || !element.index_buffer->isGpuReady()) {
                    continue;
                }
                const auto* material_instance = batch.material_instance;
                if (!material_instance) {
                    continue;
                }
                auto* material_system = m_shared_render_service ? m_shared_render_service->getMaterialSystem() : nullptr;
                if (!RenderSettings::IsBindlessActive()) {
                    const auto material_binding_set = material_system
                        ? material_system->getTextureBindingSet(material_instance)
                        : GfxBindingSetHandle{};
                    if (!material_binding_set || !material_binding_set->isGpuReady()) {
                        continue;
                    }
                }
                picked_element = &element;
                break;
            }
            if (!picked_element) {
                continue;
            }

            ++slot;
            PrimitiveMeshDrawShaderData shader_data{};
            shader_data.draw_data.w = static_cast<Int32>(slot);
            m_command_list->writeBuffer(m_primitive_cb.Get(), &shader_data, sizeof(shader_data));
            out_pick_ids.push_back(primitive->getId().value());

            cutie::GraphicsState graphics_state;
            graphics_state.setPipeline(m_pipeline.Get());
            graphics_state.setFramebuffer(framebuffer);
            graphics_state.setViewport(viewport_state);
            graphics_state.addBindingSet(m_view_binding_set.Get());
            graphics_state.addBindingSet(m_primitive_binding_set.Get());
            graphics_state.addVertexBuffer(
                cutie::VertexBufferBinding().setBuffer(picked_element->vertex_buffer->getRHI()).setSlot(0).setOffset(0));
            graphics_state.addVertexBuffer(
                cutie::VertexBufferBinding().setBuffer(instance_buffer).setSlot(1).setOffset(instance_offset));
            graphics_state.setIndexBuffer(
                cutie::IndexBufferBinding()
                    .setBuffer(picked_element->index_buffer->getRHI())
                    .setFormat(GfxFormat::R32_UINT)
                    .setOffset(0));
            m_command_list->setGraphicsState(graphics_state);
            m_command_list->drawIndexed(GfxDrawArguments()
                .setVertexCount(picked_element->index_count)
                .setInstanceCount(picked_element->instance_count)
                .setStartIndexLocation(picked_element->index_offset)
                .setStartVertexLocation(picked_element->vertex_offset));
        }
        return slot > 0;
    }

} // namespace dodoe
