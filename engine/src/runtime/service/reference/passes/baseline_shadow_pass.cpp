// do@Redlive

#include "baseline_shadow_pass.h"

#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_scene/light_scene_info.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/core/math/math.h"

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

    Bool BaselineShadowPass::initialize(const BaselinePassContext& context) {
        m_device = context.device;
        m_command_list = context.command_list;
        m_shader_library = context.shader_library;
        m_shared_render_service = context.shared_render_service;

        if (!m_shared_render_service || !m_shader_library) {
            return true;
        }
        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        auto* input_layout_cache = m_shared_render_service->getInputLayoutCache();
        DO_ASSERT(binding_layout_cache != nullptr, "BaselineShadowPass: binding layout cache is unavailable");
        DO_ASSERT(input_layout_cache != nullptr, "BaselineShadowPass: input layout cache is unavailable");

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

        m_input_layout = input_layout_cache->getOrCreate(BuildMeshVertexAttributes(), m_shader_library->getShadowVertexShader());

        GfxBufferDesc cb_desc;
        cb_desc.setByteSize(static_cast<UInt32>(sizeof(GlobalMeshShaderData)))
            .setIsConstantBuffer(true)
            .setIsVolatile(true)
            .setMaxVersions(kVolatileConstantBufferVersions)
            .setDebugName("BaselineShadowGlobalCB");
        m_global_cb = m_device->createBuffer(cb_desc);
        cb_desc.setByteSize(static_cast<UInt32>(sizeof(ViewMeshShaderData)))
            .setDebugName("BaselineShadowViewCB");
        m_view_cb = m_device->createBuffer(cb_desc);

        m_global_binding_set = m_device->createBindingSet(
            GfxBindingSetDesc().addItem(
                GfxBindingSetItem::ConstantBuffer(shader_bindings::kGlobalBindingConstants, m_global_cb.Get())),
            m_global_binding_layout.Get());
        m_view_binding_set = m_device->createBindingSet(
            GfxBindingSetDesc().addItem(
                GfxBindingSetItem::ConstantBuffer(shader_bindings::kViewBindingConstants, m_view_cb.Get())),
            m_view_binding_layout.Get());
        return true;
    }

    void BaselineShadowPass::shutdown() {
        m_pipeline = nullptr;
        m_input_layout = nullptr;
        m_global_binding_layout = nullptr;
        m_view_binding_layout = nullptr;
        m_global_cb = nullptr;
        m_view_cb = nullptr;
        m_global_binding_set = nullptr;
        m_view_binding_set = nullptr;
        m_instance_buffer = nullptr;
        m_instance_capacity = 0;
        m_shadow_depth = nullptr;
        m_shadow_framebuffer = nullptr;
        m_shader_library = nullptr;
        m_shared_render_service = nullptr;
        m_command_list = nullptr;
        m_device = nullptr;
    }

    Bool BaselineShadowPass::ensureShadowTarget() {
        if (m_shadow_depth && m_shadow_depth->isGpuReady() &&
            m_shadow_framebuffer && m_shadow_framebuffer->isGpuReady()) {
            return true;
        }
        GfxTextureDesc desc;
        desc.setDimension(GfxTextureDimension::Texture2D)
            .setFormat(GfxFormat::D32)
            .setWidth(kShadowMapSize)
            .setHeight(kShadowMapSize)
            .setIsRenderTarget(true)
            .enableAutomaticStateTracking(GfxResourceStates::DepthWrite)
            .setDebugName("BaselineShadowMap");
        m_shadow_depth = create_ref<GfxTexture>(desc, "BaselineShadowMap");
        m_shadow_depth->initializeGpu(m_device);

        GfxFramebufferDesc fb_desc;
        fb_desc.setDepthAttachment(m_shadow_depth);
        m_shadow_framebuffer = create_ref<GfxFramebuffer>(fb_desc);
        m_shadow_framebuffer->initializeGpu(m_device);
        return m_shadow_depth->isGpuReady() && m_shadow_framebuffer->isGpuReady();
    }

    void BaselineShadowPass::ensurePipeline() {
        if (m_pipeline) {
            return;
        }
        if (!m_shader_library || !m_shared_render_service || !m_device) {
            return;
        }
        if (!ensureShadowTarget()) {
            return;
        }
        const auto vertex_shader = m_shader_library->getShadowVertexShader();
        const auto pixel_shader = m_shader_library->getShadowPixelShader();
        if (!vertex_shader || !pixel_shader) {
            DO_ERROR("BaselineShadowPass: shadow shaders are not loaded");
            return;
        }

        GfxGraphicsPipelineDesc pipeline_desc;
        pipeline_desc.setPrimType(GfxPrimitiveType::TriangleList);
        pipeline_desc.setVertexShader(vertex_shader.Get());
        pipeline_desc.setPixelShader(pixel_shader.Get());
        pipeline_desc.setInputLayout(m_input_layout.Get());
        pipeline_desc.addBindingLayout(m_global_binding_layout.Get());
        pipeline_desc.addBindingLayout(m_view_binding_layout.Get());

        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(GfxComparisonFunc::Less).disableStencil();
        GfxRasterState raster_state;
        raster_state.setCullNone().setDepthBiasClamp(0.0f).setDepthBias(6).setSlopeScaleDepthBias(1.5f);
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state)
            .setRasterState(raster_state);
        pipeline_desc.setRenderState(render_state);

        m_pipeline = m_device->createGraphicsPipeline(pipeline_desc, m_shadow_framebuffer->getFramebufferInfo().getRHI());
        DO_INFO("BaselineShadowPass: render pipeline created");
    }

    void BaselineShadowPass::ensureInstanceCapacity(UInt32 instance_count) {
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
            .setDebugName("BaselineShadowInstances");
        m_instance_buffer = m_device->createBuffer(desc);
        m_instance_capacity = capacity;
    }

    BaselineShadowResult BaselineShadowPass::render(RenderView& view, RenderScene& scene) {
        BaselineShadowResult result{};
        if (!m_pipeline || !m_shadow_depth || !m_shadow_depth->isGpuReady()) {
            return result;
        }
        static UInt64 s_shadow_debug_frame = 0;
        const Bool shadow_debug = ((s_shadow_debug_frame++) % 120) == 0;
        const auto* mesh_ext = view.getExtension<MeshViewExtension>();
        if (!mesh_ext || mesh_ext->instance_scene_data.empty()) {
            if (shadow_debug) {
                DO_INFO("BaselineShadowPass: draw skipped (ext={} instances={})",
                    mesh_ext != nullptr, mesh_ext ? mesh_ext->instance_scene_data.size() : 0);
            }
            return result;
        }

        const LightSceneInfo* directional = nullptr;
        for (const auto& info : scene.getLightSceneInfos()) {
            if (info.getLightType() == LightType::Directional && info.isEnabled()) {
                directional = &info;
                break;
            }
        }
        if (!directional) {
            if (shadow_debug) {
                DO_INFO("BaselineShadowPass: draw skipped (no enabled directional light)");
            }
            return result;
        }

        const auto& primitive_indices = mesh_ext->getMeshPassPrimitiveIndices(MeshPassType::Shadow);
        if (primitive_indices.empty()) {
            if (shadow_debug) {
                DO_INFO("BaselineShadowPass: draw skipped (no shadow indices, primitives={})",
                    mesh_ext->visible_primitives.size());
            }
            return result;
        }
        result.has_shadow = true;

        Vector3f bounds_min(0.0f);
        Vector3f bounds_max(0.0f);
        Bool bounds_valid = false;
        for (const auto* primitive : mesh_ext->visible_primitives) {
            if (!primitive) {
                continue;
            }
            const Vector3f& p_min = primitive->getBoundsMin();
            const Vector3f& p_max = primitive->getBoundsMax();
            if (!bounds_valid) {
                bounds_min = p_min;
                bounds_max = p_max;
                bounds_valid = true;
                continue;
            }
            bounds_min = Math::Min(bounds_min, p_min);
            bounds_max = Math::Max(bounds_max, p_max);
        }
        const Vector3f bounds_center = (bounds_min + bounds_max) * 0.5f;
        const Float bounds_extent = Math::Length(Math::Max(bounds_max - bounds_center, Vector3f(0.0f)));
        if (shadow_debug) {
            DO_INFO("BaselineShadowPass: primitives={} shadow_indices={} instances={} center=({:.1f},{:.1f},{:.1f}) extent={:.1f}",
                mesh_ext->visible_primitives.size(), primitive_indices.size(),
                mesh_ext->instance_scene_data.size(),
                bounds_center.x, bounds_center.y, bounds_center.z, bounds_extent);
        }

        result.light_view_projection = rendering_pipeline_utils::BuildDirectionalLightViewProjection(
            directional->getDirectionalLightData().direction, bounds_center, bounds_extent * 1.2f);
        result.shadow_map = m_shadow_depth;

        ensureInstanceCapacity(static_cast<UInt32>(mesh_ext->instance_scene_data.size()));

        m_command_list->setBufferState(m_instance_buffer.Get(), cutie::ResourceStates::CopyDest);
        m_command_list->commitBarriers();
        m_command_list->writeBuffer(m_instance_buffer.Get(), mesh_ext->instance_scene_data.data(),
            mesh_ext->instance_scene_data.size() * sizeof(InstanceSceneData));

        const GlobalMeshShaderData global_data{mesh_ext->frame_time_data};
        m_command_list->writeBuffer(m_global_cb.Get(), &global_data, sizeof(global_data));
        const ViewMeshShaderData view_data{result.light_view_projection};
        m_command_list->writeBuffer(m_view_cb.Get(), &view_data, sizeof(view_data));

        m_command_list->setBufferState(m_instance_buffer.Get(), cutie::ResourceStates::VertexBuffer);
        m_command_list->commitBarriers();

        DynamicArray<UInt32> instance_prefix(mesh_ext->visible_primitives.size() + 1, 0);
        for (Size_t i = 0; i < mesh_ext->visible_primitives.size(); ++i) {
            const auto* primitive = mesh_ext->visible_primitives[i];
            instance_prefix[i + 1] = instance_prefix[i] + (primitive ? primitive->getInstanceCount() : 0);
        }

        const auto viewport_state = GfxViewportState().addViewportAndScissorRect(
            GfxViewport(0.0f, static_cast<Float>(kShadowMapSize), 0.0f, static_cast<Float>(kShadowMapSize), 0.0f, 1.0f));

        UInt32 shadow_draw_count = 0;
        UInt32 shadow_skip_batch = 0;
        UInt32 shadow_skip_buffer = 0;
        m_command_list->setTextureState(m_shadow_depth->getRHI(), cutie::AllSubresources, cutie::ResourceStates::DepthWrite);
        m_command_list->commitBarriers();
        m_command_list->clearDepthStencilTexture(m_shadow_depth->getRHI(), cutie::AllSubresources, true, 1.0f, false, 0);

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
                if (!batch.isValid() || !batch.isRelevant(MeshPassType::Shadow) || batch.getElements().empty()) {
                    shadow_skip_batch++;
                    continue;
                }
                const auto& element = batch.getElements()[0];
                if (!element.isValid() || !element.vertex_buffer || !element.index_buffer ||
                    !element.vertex_buffer->isGpuReady() || !element.index_buffer->isGpuReady()) {
                    shadow_skip_buffer++;
                    continue;
                }

                cutie::GraphicsState graphics_state;
                graphics_state.setPipeline(m_pipeline.Get());
                graphics_state.setFramebuffer(m_shadow_framebuffer->getRHI());
                graphics_state.setViewport(viewport_state);
                graphics_state.addBindingSet(m_global_binding_set.Get());
                graphics_state.addBindingSet(m_view_binding_set.Get());
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
                m_command_list->drawIndexed(GfxDrawArguments()
                    .setVertexCount(element.index_count)
                    .setInstanceCount(element.instance_count)
                    .setStartIndexLocation(element.index_offset)
                    .setStartVertexLocation(element.vertex_offset));
                shadow_draw_count++;
            }
        }
        if (shadow_debug) {
            DO_INFO("BaselineShadowPass: draws={} skip_batch={} skip_buffer={}",
                shadow_draw_count, shadow_skip_batch, shadow_skip_buffer);
        }

        m_command_list->setTextureState(m_shadow_depth->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
        m_command_list->commitBarriers();
        return result;
    }

} // namespace dodoe
