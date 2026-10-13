// do@Redlive

#include "render_pick_pass.h"

#ifdef DODOE_EDITOR_ENABLED

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../render_pipeline_pass_utils.h"

#include "runtime/core/channel/pick_channel.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_view/sprite_view_extension.h"
#include "runtime/function/render/render_scene/sprite_scene_info.h"
#include "runtime/function/render/render_scene/primitive_scene_info.h"
#include "runtime/function/render/mesh_draw/mesh_batch.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/pipeline_state/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/core/math/math.h"

#include <algorithm>
#include <cmath>

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

        UInt64 RaycastSprites(const RenderView& view, Float px, Float py) {
            const auto* sprite_ext = view.getExtension<SpriteViewExtension>();
            if (sprite_ext->visible_sprites.empty()) {
                return 0;
            }
            const auto vp = view.getViewportRect();
            const Float vp_w = vp.z > 0 ? static_cast<Float>(vp.z) : 1.0f;
            const Float vp_h = vp.w > 0 ? static_cast<Float>(vp.w) : 1.0f;
            const Float ndc_x = (px - static_cast<Float>(vp.x)) / vp_w * 2.0f - 1.0f;
            const Float ndc_y = (py - static_cast<Float>(vp.y)) / vp_h * 2.0f - 1.0f;
            const Matrix4f inv_vp = Math::Inverse(view.getViewProjectionMatrix());
            const Vector4f near_h = inv_vp * Vector4f(ndc_x, ndc_y, 0.0f, 1.0f);
            const Vector4f far_h = inv_vp * Vector4f(ndc_x, ndc_y, 1.0f, 1.0f);
            if (std::abs(near_h.w) < 1e-6f || std::abs(far_h.w) < 1e-6f) {
                return 0;
            }
            const Vector3f near_p = Vector3f(near_h) / near_h.w;
            const Vector3f far_p = Vector3f(far_h) / far_h.w;
            const Vector3f dir = far_p - near_p;
            if (std::abs(dir.z) < 1e-8f) {
                return 0;
            }
            const Float t = -near_p.z / dir.z;
            if (!(t >= 0.0f) || !(t <= 1.0f)) {
                return 0;
            }
            const Vector2f point(near_p.x + dir.x * t, near_p.y + dir.y * t);

            const SpriteSceneInfo* best = nullptr;
            for (const auto* sprite : sprite_ext->visible_sprites) {
                if (sprite->hasInstances()) {
                    const Vector3f c = sprite->getBoundsCenter();
                    const Vector3f e = sprite->getBoundsExtents();
                    if (std::abs(point.x - c.x) > std::max(e.x, 1e-4f) ||
                        std::abs(point.y - c.y) > std::max(e.y, 1e-4f)) {
                        continue;
                    }
                } else {
                    const Vector2f p = sprite->getPosition();
                    const Vector2f s = sprite->getScale();
                    const Float c = std::cos(sprite->getRotation());
                    const Float sn = std::sin(sprite->getRotation());
                    const Vector2f d = point - p;
                    const Vector2f local(c * d.x + sn * d.y, -sn * d.x + c * d.y);
                    if (std::abs(local.x) > std::abs(s.x) * 0.5f ||
                        std::abs(local.y) > std::abs(s.y) * 0.5f) {
                        continue;
                    }
                }
                if (!best || sprite->getSortingKey() > best->getSortingKey()) {
                    best = sprite;
                }
            }
            return best ? best->getId().value() : 0;
        }
    }

    void EditorPickPass::ensureCpuResources(const RenderPassBuildContext& context) {
        if (m_view_binding_layout && m_primitive_binding_layout && m_input_layout) {
            return;
        }
        auto* binding_layout_cache = context.shared_render_service->getBindingLayoutCache();
        auto* input_layout_cache = context.shared_render_service->getInputLayoutCache();
        const auto pick_vertex_shader = context.shared_render_service->getShaderLibrary()->getPickVertexShader();
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
        m_input_layout = input_layout_cache->getOrCreate(BuildPickVertexAttributes(), pick_vertex_shader);
    }

    Bool EditorPickPass::ensureGpuResources(const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
        if (!m_view_cb) {
            GfxBufferDesc cb_desc;
            cb_desc.setByteSize(static_cast<UInt32>(sizeof(ViewMeshShaderData)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("EditorPickViewCB");
            m_view_cb = command_list.createBuffer(cb_desc);
        }
        if (!m_primitive_cb) {
            GfxBufferDesc cb_desc;
            cb_desc.setByteSize(static_cast<UInt32>(sizeof(PrimitiveMeshDrawShaderData)))
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kVolatileConstantBufferVersions)
                .setDebugName("EditorPickPrimitiveCB");
            m_primitive_cb = command_list.createBuffer(cb_desc);
        }
        if (!m_view_binding_set && m_view_cb) {
            m_view_binding_set = command_list.createBindingSet(
                GfxBindingSetDesc().addItem(GfxBindingSetItem::ConstantBuffer(
                    shader_bindings::kViewBindingConstants, m_view_cb->getRHIHandle())),
                m_view_binding_layout);
        }
        if (!m_primitive_binding_set && m_primitive_cb) {
            m_primitive_binding_set = command_list.createBindingSet(
                GfxBindingSetDesc().addItem(GfxBindingSetItem::ConstantBuffer(
                    shader_bindings::kPrimitiveBindingConstants, m_primitive_cb->getRHIHandle())),
                m_primitive_binding_layout);
        }
        return m_view_cb && m_primitive_cb && m_view_binding_set && m_primitive_binding_set;
    }

    void EditorPickPass::resolvePendingCopy() {
        m_copy_in_flight = false;
        UInt32 slot = 0;
        Size_t row_pitch = 0;
        if (void* data = GDrawCommandList.mapStagingTexture(m_staging_texture,
                GfxTextureSlice().setSize(1, 1), GfxCpuAccessMode::Read, &row_pitch)) {
            slot = *static_cast<const UInt32*>(data);
            GDrawCommandList.unmapStagingTexture(m_staging_texture);
        }
        UInt64 entity_uuid = 0;
        if (slot != 0 && static_cast<Size_t>(slot - 1) < m_pick_ids.size()) {
            entity_uuid = m_pick_ids[slot - 1];
        }
        if (entity_uuid == 0) {
            entity_uuid = m_sprite_pick_uuid;
        }
        DO_INFO("EditorPick: resolve seq={} slot={} uuid={}", m_pending_sequence, slot, entity_uuid);
        publishResult(m_pending_sequence, entity_uuid);
    }

    void EditorPickPass::publishResult(UInt64 sequence, UInt64 entity_uuid) {
        auto& channel = GetPickChannel().get<PickChannelData>();
        channel.result.sequence = sequence;
        channel.result.entity_uuid = entity_uuid;
        channel.result.handled = true;
    }

    void EditorPickPass::build(RenderGraphBuilder& graph,
                               const RenderPassBuildContext& context) {
        if (!context.view.hasViewFlag(RenderView::kShowEditorPrimitives)) {
            return;
        }
        ensureCpuResources(context);
        if (m_copy_in_flight) {
            if (m_resolve_delay > 0) {
                --m_resolve_delay;
            } else {
                resolvePendingCopy();
            }
        }

        auto& channel = GetPickChannel().get<PickChannelData>();
        const auto& request = channel.request;
        if (request.sequence == 0 || request.sequence == m_consumed_sequence) {
            return;
        }
        m_consumed_sequence = request.sequence;

        if (!m_staging_texture) {
            GfxTextureDesc staging_desc;
            staging_desc.setDimension(GfxTextureDimension::Texture2D)
                .setFormat(GfxFormat::R32_UINT)
                .setWidth(1)
                .setHeight(1);
            m_staging_texture = GDrawCommandList.createStagingTexture(staging_desc, GfxCpuAccessMode::Read);
        }
        const auto* mesh_ext = context.view.getExtension<MeshViewExtension>();
        const auto& primitive_indices = mesh_ext
            ? mesh_ext->getMeshPassPrimitiveIndices(MeshPassType::Opaque)
            : DynamicArray<UInt32>{};
        const Bool pickable = request.x >= 0 && request.y >= 0 && mesh_ext &&
            !mesh_ext->instance_scene_data.empty() && !primitive_indices.empty();
        m_sprite_pick_uuid =
            RaycastSprites(context.view, static_cast<Float>(request.x), static_cast<Float>(request.y));
        if (!pickable) {
            publishResult(request.sequence, m_sprite_pick_uuid);
            return;
        }

        m_active_x = static_cast<UInt32>(request.x);
        m_active_y = static_cast<UInt32>(request.y);
        m_pick_ids.clear();

        graph.addPass<PickPassParameters>(
            "EditorPickPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [this, &context](RenderGraphPassBuilder& pass_builder, PickPassParameters& parameters) {
                const auto* scene_textures = pass_builder.blackboard().get<SceneTexturesKey>();
                RenderGraphAttachmentInfo depth_attachment{};
                depth_attachment.load_op = LoadOp::Load;
                parameters.depth = pass_builder.writeDepth(scene_textures->depth, depth_attachment);
                parameters.instance_buffer = pass_builder.read(scene_textures->instance_scene_data);
                RenderGraphAttachmentInfo pick_attachment{};
                pick_attachment.load_op = LoadOp::DontCare;
                const auto extent = context.gfx_context->getSwapchainExtent2D();
                parameters.pick_target = pass_builder.writeColor(pass_builder.createTransientTexture(
                    MakeRenderTarget2D(
                        static_cast<UInt32>(extent.x), static_cast<UInt32>(extent.y),
                        GfxFormat::R32_UINT, "RDG EditorPickTarget"),
                    "EditorPickTarget"), pick_attachment);
                pass_builder.exportTexture(parameters.pick_target, GfxResourceStates::CopySource);

                const auto vp = context.view.getViewportRect();
                const Int32 vp_w = vp.z > 0 ? vp.z : static_cast<Int32>(extent.x);
                const Int32 vp_h = vp.w > 0 ? vp.w : static_cast<Int32>(extent.y);
                const Int32 local_x = std::clamp(static_cast<Int32>(m_active_x) - vp.x, 0, vp_w - 1);
                const Int32 local_y = std::clamp(static_cast<Int32>(m_active_y) - vp.y, 0, vp_h - 1);
                parameters.pick_x = static_cast<UInt32>(vp.x + local_x);
                parameters.pick_y = static_cast<UInt32>(vp.y + (vp_h - 1 - local_y));
            },
            [this](const PickPassParameters& parameters,
                   const RenderGraphPassContext& ctx,
                   DrawCommandList& command_list) {
                executePick(parameters, ctx, command_list);
            });
    }

    void EditorPickPass::executePick(const PickPassParameters& parameters,
                                     const RenderGraphPassContext& ctx,
                                     DrawCommandList& command_list) {
        if (!ensureGpuResources(ctx, command_list)) {
            return;
        }
        const auto pick_target = ctx.resolveTexture(parameters.pick_target);
        const auto instance_buffer = ctx.resolveBuffer(parameters.instance_buffer);
        auto* shader_library = ctx.getShaderLibrary();
        auto* mesh_ext = ctx.getView()->getExtension<MeshViewExtension>();
        const auto vertex_shader = shader_library->getPickVertexShader();
        const auto pixel_shader = shader_library->getPickPixelShader();
        const auto& primitive_indices = mesh_ext->getMeshPassPrimitiveIndices(MeshPassType::Opaque);

        command_list.writeBuffer(m_view_cb, ViewMeshShaderData{ctx.getView()->getViewProjectionMatrix()});
        command_list.clearTextureUInt(pick_target, GfxAllSubresources, 0);

        DynamicArray<UInt32> instance_prefix(mesh_ext->visible_primitives.size() + 1, 0);
        for (Size_t i = 0; i < mesh_ext->visible_primitives.size(); ++i) {
            const auto* primitive = mesh_ext->visible_primitives[i];
            instance_prefix[i + 1] = instance_prefix[i] + (primitive ? primitive->getInstanceCount() : 0);
        }

        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.enableDepthTest().disableDepthWrite().setDepthFunc(GfxComparisonFunc::LessOrEqual).disableStencil();
        GfxRasterState raster_state;
        raster_state.setCullNone();
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state).setRasterState(raster_state);

        const auto pipeline = ctx.getPipelineStateCache()->resolveGraphicsPipeline(
            GfxGraphicsPipelineDesc()
                .setVertexShader(vertex_shader)
                .setPixelShader(pixel_shader)
                .setInputLayout(m_input_layout)
                .addBindingLayout(m_view_binding_layout)
                .addBindingLayout(m_primitive_binding_layout)
                .setPrimType(GfxPrimitiveType::TriangleList)
                .setRenderState(render_state),
            ctx.getRenderTargetSignature(),
            command_list);
        if (!pipeline) {
            DO_ERROR("EditorPickPass: failed to create pipeline");
            return;
        }

        const auto viewport_state = rendering_pipeline_utils::BuildViewportState(
            *ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2D());
        DynamicArray<GfxBindingSetHandle> binding_sets = {m_view_binding_set, m_primitive_binding_set};

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
                if (!batch.isValid() || !batch.isRelevant(MeshPassType::Opaque) || batch.getElements().empty()) {
                    continue;
                }
                const auto& element = batch.getElements()[0];
                if (!element.isValid() || !element.vertex_buffer || !element.index_buffer ||
                    !element.vertex_buffer->isGpuReady() || !element.index_buffer->isGpuReady()) {
                    continue;
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
            command_list.writeBuffer(m_primitive_cb, shader_data);
            m_pick_ids.push_back(primitive->getId().value());

            DynamicArray<GfxVertexBufferBinding> vertex_buffers;
            vertex_buffers.push_back(GfxVertexBufferBinding()
                .setBuffer(picked_element->vertex_buffer->getRHIHandle()).setSlot(0).setOffset(0));
            vertex_buffers.push_back(GfxVertexBufferBinding()
                .setBuffer(instance_buffer->getRHIHandle()).setSlot(1).setOffset(instance_offset));
            GfxIndexBufferBinding index_binding;
            index_binding.setBuffer(picked_element->index_buffer->getRHIHandle())
                .setFormat(GfxFormat::R32_UINT)
                .setOffset(0);

            command_list.setGraphicsState(ctx.getFramebuffer(), pipeline, binding_sets, viewport_state,
                vertex_buffers, index_binding);
            command_list.drawIndexed(GfxDrawArguments()
                .setVertexCount(picked_element->index_count)
                .setInstanceCount(picked_element->instance_count)
                .setStartIndexLocation(picked_element->index_offset)
                .setStartVertexLocation(picked_element->vertex_offset));
        }

        if (slot > 0) {
            command_list.setTextureState(pick_target, GfxAllSubresources, GfxResourceStates::CopySource);
            command_list.commitBarriers();
            command_list.copyTextureToStaging(m_staging_texture,
                GfxTextureSlice().setSize(1, 1), pick_target,
                GfxTextureSlice().setOrigin(parameters.pick_x, parameters.pick_y).setSize(1, 1));
            m_pending_sequence = m_consumed_sequence;
            m_copy_in_flight = true;
            m_resolve_delay = 2;
        } else {
            publishResult(m_consumed_sequence, m_sprite_pick_uuid);
        }
    }

} // namespace dodoe

#endif // DODOE_EDITOR_ENABLED
