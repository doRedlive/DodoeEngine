// do@Redlive

#include "baseline_renderer.h"

#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_view/render_view_family.h"
#include "runtime/function/render/render_view/sprite_view_extension.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_scene/light_scene_info.h"
#include "runtime/function/render/texture/texture_manager.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/function/render/gpu_driven/gpu_scene.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"
#include "runtime/function/render/mesh_draw/mesh_batch.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/function/render/render_pipeline/passes/render_opaque_pass.h"
#include "runtime/function/render/material/material_system.h"

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

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

    Bool BaselineRenderer::initialize(const BaselineRendererCreateInfo& info) {
        if (!info.device) {
            DO_ERROR("BaselineRenderer: device is null");
            return false;
        }
        m_device = info.device;
        m_shader_library = info.shader_library;
        m_shared_render_service = info.shared_render_service;
        m_command_list = m_device->createCommandList();
        if (!m_command_list) {
            DO_ERROR("BaselineRenderer: failed to create raw command list");
            return false;
        }
        if (!createSpriteResources()) {
            return false;
        }
        if (!createMeshResources()) {
            return false;
        }
        if (!createPresentResources()) {
            return false;
        }
        m_frame_counter = 0;
        DO_INFO("BaselineRenderer: initialized (raw cutie path, 2d + 3d scene)");
        return true;
    }

    Bool BaselineRenderer::createSpriteResources() {
        if (!m_shared_render_service) {
            DO_INFO("BaselineRenderer: shared render service unavailable, 2d scene drawing disabled");
            return true;
        }
        if (!m_shader_library) {
            DO_ERROR("BaselineRenderer: shader library is unavailable for sprite resources");
            return false;
        }

        GfxBindingLayoutDesc cb_desc;
        cb_desc.setVisibility(GfxShaderType::Vertex | GfxShaderType::Pixel)
            .setRegisterSpaceIsDescriptorSet(true)
            .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::View))
            .addItem(GfxBindingLayoutItem::ConstantBuffer(0));
        m_sprite_cb_binding_layout = m_device->createBindingLayout(cb_desc);
        if (!m_sprite_cb_binding_layout) {
            DO_ERROR("BaselineRenderer: failed to create sprite constant buffer binding layout");
            return false;
        }

        GfxBindingLayoutDesc material_desc;
        material_desc.setVisibility(GfxShaderType::Vertex | GfxShaderType::Pixel)
            .setRegisterSpaceIsDescriptorSet(true)
            .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Material))
            .addItem(GfxBindingLayoutItem::Texture_SRV(2))
            .addItem(GfxBindingLayoutItem::Sampler(1));
        m_sprite_material_binding_layout = m_device->createBindingLayout(material_desc);
        if (!m_sprite_material_binding_layout) {
            DO_ERROR("BaselineRenderer: failed to create sprite material binding layout");
            return false;
        }

        auto* input_layout_cache = m_shared_render_service->getInputLayoutCache();
        if (!input_layout_cache) {
            DO_ERROR("BaselineRenderer: input layout cache is unavailable");
            return false;
        }
        const DynamicArray<GfxVertexAttributeDesc> attributes = {
            GfxVertexAttributeDesc().setName("POSITION").setFormat(GfxFormat::RGB32_FLOAT).setOffset(0).setElementStride(sizeof(QuadVertex)),
            GfxVertexAttributeDesc().setName("TEXCOORD").setFormat(GfxFormat::RG32_FLOAT).setOffset(sizeof(Vector3f)).setElementStride(sizeof(QuadVertex)),
            GfxVertexAttributeDesc().setName("COLOR").setFormat(GfxFormat::RGBA8_UNORM).setOffset(sizeof(Vector3f) + sizeof(Vector2f)).setElementStride(sizeof(QuadVertex)),
            GfxVertexAttributeDesc().setName("TEXINDEX").setFormat(GfxFormat::R32_UINT).setOffset(sizeof(Vector3f) + sizeof(Vector2f) + sizeof(UInt32)).setElementStride(sizeof(QuadVertex)),
            GfxVertexAttributeDesc().setName("TEXCOORD1").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(0).setElementStride(sizeof(SpriteInstance)).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD2").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f)).setElementStride(sizeof(SpriteInstance)).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD3").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f) * 2).setElementStride(sizeof(SpriteInstance)).setIsInstanced(true),
            GfxVertexAttributeDesc().setName("TEXCOORD4").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f) * 3).setElementStride(sizeof(SpriteInstance)).setIsInstanced(true),
        };
        m_sprite_input_layout = input_layout_cache->getOrCreate(attributes, m_shader_library->getSpriteVertexShader());
        if (!m_sprite_input_layout) {
            DO_ERROR("BaselineRenderer: failed to create sprite input layout");
            return false;
        }

        GfxBufferDesc vp_desc;
        vp_desc.setByteSize(sizeof(Matrix4f))
            .setIsConstantBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
            .setDebugName("BaselineSpriteVP");
        m_vp_buffer = m_device->createBuffer(vp_desc);
        if (!m_vp_buffer) {
            DO_ERROR("BaselineRenderer: failed to create sprite vp buffer");
            return false;
        }

        GfxSamplerDesc sampler_desc;
        sampler_desc.setAllFilters(true)
            .setAllAddressModes(GfxSamplerAddressMode::Clamp);
        m_sampler = m_device->createSampler(sampler_desc);
        if (!m_sampler) {
            DO_ERROR("BaselineRenderer: failed to create sampler");
            return false;
        }
        return true;
    }

    Bool BaselineRenderer::createMeshResources() {
        if (!m_shared_render_service || !m_shader_library) {
            return true;
        }
        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        if (!binding_layout_cache) {
            DO_ERROR("BaselineRenderer: binding layout cache is unavailable");
            return false;
        }
        auto* input_layout_cache = m_shared_render_service->getInputLayoutCache();
        if (!input_layout_cache) {
            DO_ERROR("BaselineRenderer: input layout cache is unavailable");
            return false;
        }

        m_mesh_global_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Global))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kGlobalBindingConstants)));
        m_mesh_view_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::View))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kViewBindingConstants)));
        m_mesh_primitive_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::All)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Primitive))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(shader_bindings::kPrimitiveBindingConstants)));
        m_mesh_material_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Pixel)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Material))
                .addItem(GfxBindingLayoutItem::Sampler(shader_bindings::kMaterialBindingSampler))
                .addItem(GfxBindingLayoutItem::Texture_SRV(shader_bindings::kMaterialBindingBaseColor))
                .addItem(GfxBindingLayoutItem::Texture_SRV(shader_bindings::kMaterialBindingMetallicRough)));
        m_mesh_pass_binding_layout = MakeLitPassBindingLayout(*binding_layout_cache);
        if (!m_mesh_global_binding_layout || !m_mesh_view_binding_layout ||
            !m_mesh_primitive_binding_layout || !m_mesh_material_binding_layout || !m_mesh_pass_binding_layout) {
            DO_ERROR("BaselineRenderer: failed to create mesh binding layouts");
            return false;
        }

        m_mesh_input_layout = input_layout_cache->getOrCreate(BuildMeshVertexAttributes(), m_shader_library->getLitVertexShader());
        if (!m_mesh_input_layout) {
            DO_ERROR("BaselineRenderer: failed to create mesh input layout");
            return false;
        }

        GfxBufferDesc cb_desc;
        cb_desc.setByteSize(static_cast<UInt32>(sizeof(GlobalMeshShaderData)))
            .setIsConstantBuffer(true)
            .setIsVolatile(true)
            .setMaxVersions(kVolatileConstantBufferVersions)
            .setDebugName("BaselineMeshGlobalCB");
        m_mesh_global_cb = m_device->createBuffer(cb_desc);
        cb_desc.setByteSize(static_cast<UInt32>(sizeof(ViewMeshShaderData)))
            .setDebugName("BaselineMeshViewCB");
        m_mesh_view_cb = m_device->createBuffer(cb_desc);
        cb_desc.setByteSize(static_cast<UInt32>(sizeof(PrimitiveMeshDrawShaderData)))
            .setDebugName("BaselineMeshPrimitiveCB");
        m_mesh_primitive_cb = m_device->createBuffer(cb_desc);
        cb_desc.setByteSize(static_cast<UInt32>(kLitPassConstantBufferSize))
            .setIsVolatile(false)
            .setMaxVersions(1)
            .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
            .setDebugName("BaselineMeshPassCB");
        m_mesh_pass_cb = m_device->createBuffer(cb_desc);
        if (!m_mesh_global_cb || !m_mesh_view_cb || !m_mesh_primitive_cb || !m_mesh_pass_cb) {
            DO_ERROR("BaselineRenderer: failed to create mesh constant buffers");
            return false;
        }

        m_mesh_global_binding_set = m_device->createBindingSet(
            GfxBindingSetDesc().addItem(
                GfxBindingSetItem::ConstantBuffer(shader_bindings::kGlobalBindingConstants, m_mesh_global_cb.Get())),
            m_mesh_global_binding_layout.Get());
        m_mesh_view_binding_set = m_device->createBindingSet(
            GfxBindingSetDesc().addItem(
                GfxBindingSetItem::ConstantBuffer(shader_bindings::kViewBindingConstants, m_mesh_view_cb.Get())),
            m_mesh_view_binding_layout.Get());
        m_mesh_primitive_binding_set = m_device->createBindingSet(
            GfxBindingSetDesc().addItem(
                GfxBindingSetItem::ConstantBuffer(shader_bindings::kPrimitiveBindingConstants, m_mesh_primitive_cb.Get())),
            m_mesh_primitive_binding_layout.Get());
        if (!m_mesh_global_binding_set || !m_mesh_view_binding_set || !m_mesh_primitive_binding_set) {
            DO_ERROR("BaselineRenderer: failed to create mesh binding sets");
            return false;
        }
        return true;
    }

    Bool BaselineRenderer::createPresentResources() {
        if (!m_shared_render_service || !m_shader_library) {
            return true;
        }
        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        if (!binding_layout_cache) {
            DO_ERROR("BaselineRenderer: binding layout cache is unavailable");
            return false;
        }
        m_present_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Pixel)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                .addItem(GfxBindingLayoutItem::Sampler(9)));
        if (!m_present_binding_layout) {
            DO_ERROR("BaselineRenderer: failed to create present binding layout");
            return false;
        }
        return true;
    }

    Bool BaselineRenderer::ensureRenderTarget(const Vector2i& extent) {
        if (m_scene_framebuffer && m_scene_rt_extent == extent &&
            m_scene_color && m_scene_color->isGpuReady() &&
            m_scene_depth && m_scene_depth->isGpuReady()) {
            return true;
        }
        if (extent.x <= 0 || extent.y <= 0) {
            return false;
        }

        m_scene_framebuffer = nullptr;
        m_scene_color = nullptr;
        m_scene_depth = nullptr;

        GfxTextureDesc color_desc;
        color_desc.setDimension(GfxTextureDimension::Texture2D)
            .setFormat(GfxFormat::RGBA8_UNORM)
            .setWidth(static_cast<UInt32>(extent.x))
            .setHeight(static_cast<UInt32>(extent.y))
            .setIsRenderTarget(true)
            .enableAutomaticStateTracking(GfxResourceStates::RenderTarget)
            .setDebugName("BaselineSceneColor");
        m_scene_color = create_ref<GfxTexture>(color_desc, "BaselineSceneColor");
        m_scene_color->initializeGpu(m_device);
        if (!m_scene_color || !m_scene_color->isGpuReady()) {
            DO_ERROR("BaselineRenderer: failed to create scene color target");
            return false;
        }

        GfxTextureDesc depth_desc;
        depth_desc.setDimension(GfxTextureDimension::Texture2D)
            .setFormat(GfxFormat::D32)
            .setWidth(static_cast<UInt32>(extent.x))
            .setHeight(static_cast<UInt32>(extent.y))
            .setIsRenderTarget(true)
            .enableAutomaticStateTracking(GfxResourceStates::DepthWrite)
            .setDebugName("BaselineSceneDepth");
        m_scene_depth = create_ref<GfxTexture>(depth_desc, "BaselineSceneDepth");
        m_scene_depth->initializeGpu(m_device);
        if (!m_scene_depth || !m_scene_depth->isGpuReady()) {
            DO_ERROR("BaselineRenderer: failed to create scene depth target");
            return false;
        }

        GfxFramebufferDesc framebuffer_desc;
        framebuffer_desc.addColorAttachment(m_scene_color);
        framebuffer_desc.setDepthAttachment(m_scene_depth);
        m_scene_framebuffer = create_ref<GfxFramebuffer>(framebuffer_desc);
        m_scene_framebuffer->initializeGpu(m_device);
        if (!m_scene_framebuffer || !m_scene_framebuffer->isGpuReady()) {
            DO_ERROR("BaselineRenderer: failed to create scene framebuffer");
            return false;
        }
        m_scene_rt_extent = extent;
        return true;
    }

    Bool BaselineRenderer::ensureInstanceCapacity(UInt32 instance_count) {
        if (instance_count <= m_instance_capacity) {
            return true;
        }
        UInt32 capacity = m_instance_capacity > 0 ? m_instance_capacity : kInitialInstanceCapacity;
        while (capacity < instance_count) {
            capacity *= 2;
        }
        GfxBufferDesc desc;
        desc.setByteSize(static_cast<UInt64>(capacity) * sizeof(SpriteInstance))
            .setIsVertexBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
            .setDebugName("BaselineSpriteInstances");
        auto new_buffer = m_device->createBuffer(desc);
        if (!new_buffer) {
            DO_ERROR("BaselineRenderer: failed to create sprite instance buffer (capacity={})", capacity);
            return false;
        }
        m_instance_buffer = new_buffer;
        m_instance_capacity = capacity;
        return true;
    }

    Bool BaselineRenderer::ensureMeshInstanceCapacity(UInt32 instance_count) {
        if (instance_count <= m_mesh_instance_capacity) {
            return true;
        }
        UInt32 capacity = m_mesh_instance_capacity > 0 ? m_mesh_instance_capacity : kInitialInstanceCapacity;
        while (capacity < instance_count) {
            capacity *= 2;
        }
        GfxBufferDesc desc;
        desc.setByteSize(static_cast<UInt64>(capacity) * sizeof(InstanceSceneData))
            .setIsVertexBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
            .setDebugName("BaselineMeshInstances");
        auto new_buffer = m_device->createBuffer(desc);
        if (!new_buffer) {
            DO_ERROR("BaselineRenderer: failed to create mesh instance buffer (capacity={})", capacity);
            return false;
        }
        m_mesh_instance_buffer = new_buffer;
        m_mesh_instance_capacity = capacity;
        return true;
    }

    void BaselineRenderer::shutdown() {
        if (m_device) {
            m_device->waitForIdle();
        }
        m_sampler = nullptr;
        m_sprite_pipeline = nullptr;
        m_sprite_cb_binding_layout = nullptr;
        m_sprite_material_binding_layout = nullptr;
        m_sprite_input_layout = nullptr;
        m_instance_buffer = nullptr;
        m_vp_buffer = nullptr;
        m_instance_capacity = 0;
        m_mesh_pipeline = nullptr;
        m_mesh_input_layout = nullptr;
        m_mesh_global_binding_layout = nullptr;
        m_mesh_view_binding_layout = nullptr;
        m_mesh_material_binding_layout = nullptr;
        m_mesh_pass_binding_layout = nullptr;
        m_mesh_primitive_binding_layout = nullptr;
        m_mesh_global_cb = nullptr;
        m_mesh_view_cb = nullptr;
        m_mesh_primitive_cb = nullptr;
        m_mesh_pass_cb = nullptr;
        m_mesh_instance_buffer = nullptr;
        m_mesh_instance_capacity = 0;
        m_mesh_global_binding_set = nullptr;
        m_mesh_view_binding_set = nullptr;
        m_mesh_primitive_binding_set = nullptr;
        m_mesh_pass_binding_set = nullptr;
        m_mesh_material_binding_sets.clear();
        m_present_pipeline = nullptr;
        m_present_binding_layout = nullptr;
        m_scene_framebuffer = nullptr;
        m_scene_color = nullptr;
        m_scene_depth = nullptr;
        m_scene_rt_extent = Vector2i(0, 0);
        m_command_list = nullptr;
        m_shader_library = nullptr;
        m_shared_render_service = nullptr;
        m_device = nullptr;
        m_frame_counter = 0;
    }

    Bool BaselineRenderer::ensureSpritePipeline(const cutie::FramebufferInfo& framebuffer_info) {
        if (m_sprite_pipeline) {
            return true;
        }
        if (!m_shader_library || !m_shared_render_service) {
            return false;
        }
        const auto vertex_shader = m_shader_library->getSpriteVertexShader();
        const auto pixel_shader = m_shader_library->getSpritePixelShaderTraditional();
        if (!vertex_shader || !pixel_shader) {
            DO_ERROR("BaselineRenderer: sprite shaders are not loaded");
            return false;
        }

        GfxGraphicsPipelineDesc pipeline_desc;
        pipeline_desc.setPrimType(GfxPrimitiveType::TriangleList);
        pipeline_desc.setVertexShader(vertex_shader.Get());
        pipeline_desc.setPixelShader(pixel_shader.Get());
        pipeline_desc.setInputLayout(m_sprite_input_layout.Get());
        pipeline_desc.addBindingLayout(m_sprite_cb_binding_layout.Get());
        pipeline_desc.addBindingLayout(m_sprite_material_binding_layout.Get());

        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.enableDepthTest().setDepthFunc(GfxComparisonFunc::LessOrEqual).disableDepthWrite().disableStencil();
        GfxRasterState raster_state;
        raster_state.setCullNone();
        GfxBlendState blend_state;
        GfxBlendState::RenderTarget blend_target;
        blend_target.enableBlend()
            .setSrcBlend(GfxBlendFactor::SrcAlpha)
            .setDestBlend(GfxBlendFactor::OneMinusSrcAlpha)
            .setSrcBlendAlpha(GfxBlendFactor::One)
            .setDestBlendAlpha(GfxBlendFactor::OneMinusSrcAlpha);
        blend_state.setRenderTarget(0, blend_target);
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state)
            .setRasterState(raster_state)
            .setBlendState(blend_state);
        pipeline_desc.setRenderState(render_state);

        m_sprite_pipeline = m_device->createGraphicsPipeline(pipeline_desc, framebuffer_info);
        if (!m_sprite_pipeline) {
            DO_ERROR("BaselineRenderer: failed to create sprite render pipeline");
            return false;
        }
        DO_INFO("BaselineRenderer: sprite render pipeline created");
        return true;
    }

    Bool BaselineRenderer::ensureMeshPipeline(const cutie::FramebufferInfo& framebuffer_info) {
        if (m_mesh_pipeline) {
            return true;
        }
        if (!m_shader_library || !m_shared_render_service) {
            return false;
        }
        const auto vertex_shader = m_shader_library->getLitVertexShader();
        const auto pixel_shader = m_shader_library->getForwardLitPixelShaderNoBindless();
        if (!vertex_shader || !pixel_shader) {
            DO_ERROR("BaselineRenderer: lit shaders are not loaded");
            return false;
        }

        GfxGraphicsPipelineDesc pipeline_desc;
        pipeline_desc.setPrimType(GfxPrimitiveType::TriangleList);
        pipeline_desc.setVertexShader(vertex_shader.Get());
        pipeline_desc.setPixelShader(pixel_shader.Get());
        pipeline_desc.setInputLayout(m_mesh_input_layout.Get());
        pipeline_desc.addBindingLayout(m_mesh_global_binding_layout.Get());
        pipeline_desc.addBindingLayout(m_mesh_view_binding_layout.Get());
        pipeline_desc.addBindingLayout(m_mesh_material_binding_layout.Get());
        pipeline_desc.addBindingLayout(m_mesh_pass_binding_layout.Get());
        pipeline_desc.addBindingLayout(m_mesh_primitive_binding_layout.Get());

        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.enableDepthTest().enableDepthWrite().setDepthFunc(GfxComparisonFunc::Less).disableStencil();
        GfxRasterState raster_state;
        raster_state.setCullNone();
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state)
            .setRasterState(raster_state);
        pipeline_desc.setRenderState(render_state);

        m_mesh_pipeline = m_device->createGraphicsPipeline(pipeline_desc, framebuffer_info);
        if (!m_mesh_pipeline) {
            DO_ERROR("BaselineRenderer: failed to create mesh render pipeline");
            return false;
        }
        DO_INFO("BaselineRenderer: mesh render pipeline created");
        return true;
    }

    Bool BaselineRenderer::ensurePresentPipeline(const cutie::FramebufferInfo& framebuffer_info) {
        if (m_present_pipeline) {
            return true;
        }
        if (!m_shader_library) {
            return false;
        }
        const auto vertex_shader = m_shader_library->getFullscreenVertexShader();
        const auto pixel_shader = m_shader_library->getBaselinePixelShader();
        if (!vertex_shader || !pixel_shader) {
            DO_ERROR("BaselineRenderer: fullscreen shaders are not loaded");
            return false;
        }

        GfxGraphicsPipelineDesc pipeline_desc;
        pipeline_desc.setPrimType(GfxPrimitiveType::TriangleList);
        pipeline_desc.setVertexShader(vertex_shader.Get());
        pipeline_desc.setPixelShader(pixel_shader.Get());
        pipeline_desc.addBindingLayout(m_present_binding_layout.Get());

        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.disableDepthTest().disableDepthWrite().disableStencil();
        GfxRasterState raster_state;
        raster_state.setCullNone();
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state)
            .setRasterState(raster_state);
        pipeline_desc.setRenderState(render_state);

        m_present_pipeline = m_device->createGraphicsPipeline(pipeline_desc, framebuffer_info);
        if (!m_present_pipeline) {
            DO_ERROR("BaselineRenderer: failed to create present pipeline");
            return false;
        }
        DO_INFO("BaselineRenderer: present pipeline created");
        return true;
    }

    void BaselineRenderer::collectSpriteInstances(RenderView& view, RenderScene& scene,
                                                  DynamicArray<SpriteInstance>& out_instances) {
        auto* texture_manager = scene.getTextureManager();
        const auto* sprite_extension = view.getExtension<SpriteViewExtension>();
        if (!sprite_extension) {
            return;
        }
        for (const auto* sprite : sprite_extension->visible_sprites) {
            if (!sprite) {
                continue;
            }
            if (sprite->hasInstances()) {
                const auto& atlases = sprite->getBatchAtlases();
                for (const auto& base : sprite->getInstances()) {
                    SpriteInstance instance = base;
                    const Size_t atlas = instance.atlas_index;
                    const auto* texture = atlas < atlases.size() ? atlases[atlas].get() : nullptr;
                    instance.atlas_index = texture_manager ? texture_manager->resolveAtlasIndex(texture) : 0;
                    out_instances.push_back(instance);
                }
            } else {
                SpriteInstance instance = sprite->toInstance();
                instance.atlas_index = scene.resolveSpriteAtlasIndex(*sprite);
                out_instances.push_back(instance);
            }
        }
        std::stable_sort(out_instances.begin(), out_instances.end(),
            [](const SpriteInstance& a, const SpriteInstance& b) {
                if (a.sorting_key != b.sorting_key) {
                    return a.sorting_key < b.sorting_key;
                }
                return a.atlas_index < b.atlas_index;
            });
    }

    void BaselineRenderer::renderSprites(RenderView& view, RenderScene& scene,
                                         cutie::IFramebuffer* framebuffer, const GfxViewportState& viewport_state) {
        if (!m_device || !m_command_list || !m_shared_render_service || !m_sprite_pipeline) {
            return;
        }

        DynamicArray<SpriteInstance> instances;
        collectSpriteInstances(view, scene, instances);
        if (instances.empty()) {
            return;
        }
        if (!ensureInstanceCapacity(static_cast<UInt32>(instances.size()))) {
            return;
        }
        const auto* gpu_scene = scene.getGpuScene();
        if (!gpu_scene) {
            return;
        }
        const auto scene_resources = gpu_scene->getPassResources();
        if (!scene_resources.quad_vb || !scene_resources.quad_ib ||
            !scene_resources.quad_vb->isGpuReady() || !scene_resources.quad_ib->isGpuReady()) {
            return;
        }

        m_command_list->setBufferState(m_instance_buffer.Get(), cutie::ResourceStates::CopyDest);
        m_command_list->setBufferState(m_vp_buffer.Get(), cutie::ResourceStates::CopyDest);
        m_command_list->commitBarriers();
        m_command_list->writeBuffer(m_instance_buffer.Get(), instances.data(),
            instances.size() * sizeof(SpriteInstance));
        const Matrix4f view_projection = Math::FlipClipSpaceY(view.getViewProjectionMatrix());
        m_command_list->writeBuffer(m_vp_buffer.Get(), &view_projection, sizeof(view_projection));

        m_command_list->setBufferState(m_instance_buffer.Get(), cutie::ResourceStates::VertexBuffer);
        m_command_list->setBufferState(m_vp_buffer.Get(), cutie::ResourceStates::ConstantBuffer);
        m_command_list->setBufferState(scene_resources.quad_vb->getRHI(), cutie::ResourceStates::VertexBuffer);
        m_command_list->setBufferState(scene_resources.quad_ib->getRHI(), cutie::ResourceStates::IndexBuffer);
        m_command_list->commitBarriers();

        Size_t start = 0;
        while (start < instances.size()) {
            Size_t end = start + 1;
            while (end < instances.size() && instances[end].atlas_index == instances[start].atlas_index) {
                ++end;
            }

            const UInt32 slot = instances[start].atlas_index;
            const auto texture = m_shared_render_service->resolveTextureBySlot(slot);
            if (!texture || !texture->isGpuReady()) {
                start = end;
                continue;
            }

            auto cb_binding_set = m_device->createBindingSet(
                GfxBindingSetDesc().addItem(GfxBindingSetItem::ConstantBuffer(0, m_vp_buffer.Get())),
                m_sprite_cb_binding_layout.Get());
            auto material_binding_set = m_device->createBindingSet(
                GfxBindingSetDesc()
                    .addItem(GfxBindingSetItem::Texture_SRV(2, texture->getRHIHandle().Get()))
                    .addItem(GfxBindingSetItem::Sampler(1, m_sampler.Get())),
                m_sprite_material_binding_layout.Get());
            if (!cb_binding_set || !material_binding_set) {
                start = end;
                continue;
            }

            cutie::GraphicsState graphics_state;
            graphics_state.setPipeline(m_sprite_pipeline.Get());
            graphics_state.setFramebuffer(framebuffer);
            graphics_state.setViewport(viewport_state);
            graphics_state.addBindingSet(cb_binding_set.Get());
            graphics_state.addBindingSet(material_binding_set.Get());
            graphics_state.addVertexBuffer(
                cutie::VertexBufferBinding().setBuffer(scene_resources.quad_vb->getRHI()).setSlot(0).setOffset(0));
            graphics_state.addVertexBuffer(
                cutie::VertexBufferBinding().setBuffer(m_instance_buffer.Get()).setSlot(1).setOffset(0));
            graphics_state.setIndexBuffer(
                cutie::IndexBufferBinding()
                    .setBuffer(scene_resources.quad_ib->getRHI())
                    .setFormat(GfxFormat::R16_UINT)
                    .setOffset(0));

            m_command_list->setGraphicsState(graphics_state);
            m_command_list->drawIndexed(GfxDrawArguments()
                .setVertexCount(6)
                .setInstanceCount(static_cast<UInt32>(end - start))
                .setStartInstanceLocation(static_cast<UInt32>(start)));

            start = end;
        }
    }

    void BaselineRenderer::setupMeshExtension(RenderView& view, RenderViewFamily& view_family) {
        auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();
        mesh_ext.frame_time_data = Vector4f(view_family.getTimeSeconds(), view_family.getDeltaSeconds(), 0.0f, 0.0f);
        Size_t total_instance_count = 0;
        for (const auto* primitive : mesh_ext.visible_primitives) {
            total_instance_count += primitive ? primitive->getInstanceCount() : 1;
        }
        mesh_ext.instance_scene_data.clear();
        mesh_ext.instance_scene_data.reserve(total_instance_count);
        for (const auto* primitive : mesh_ext.visible_primitives) {
            if (primitive) {
                for (const auto& inst_data : primitive->getInstanceSceneData()) {
                    mesh_ext.instance_scene_data.push_back(inst_data);
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

    Bool BaselineRenderer::updateMeshPassBindingSet(RenderScene& scene) {
        const auto* texture_manager = scene.getTextureManager();

        GfxTextureHandle shadow_texture{};
        if (texture_manager) {
            if (const auto* fallback = texture_manager->getFallback()) {
                shadow_texture = fallback->getGpuHandle();
            }
        }

        GfxTextureHandle skybox_texture{};
        for (const auto& light_info : scene.getLightSceneInfos()) {
            if (light_info.getLightType() != LightType::Sky || !light_info.isEnabled()) {
                continue;
            }
            const auto& cubemap = light_info.getSkyLightData().cubemap;
            if (cubemap && cubemap->getGpuHandle() && cubemap->getGpuHandle()->isGpuReady()) {
                skybox_texture = cubemap->getGpuHandle();
            }
            break;
        }
        if (!skybox_texture && texture_manager) {
            if (const auto* fallback_cubemap = texture_manager->getFallbackCubemap()) {
                skybox_texture = fallback_cubemap->getGpuHandle();
            }
        }

        GfxBindingSetDesc pass_desc;
        pass_desc.addItem(GfxBindingSetItem::ConstantBuffer(0, m_mesh_pass_cb.Get()));
        if (shadow_texture && shadow_texture->isGpuReady()) {
            pass_desc.addItem(GfxBindingSetItem::Texture_SRV(1, shadow_texture->getRHIHandle().Get()));
        }
        pass_desc.addItem(GfxBindingSetItem::Texture_SRV(
            2, skybox_texture ? skybox_texture->getRHIHandle().Get() : nullptr,
            GfxFormat::UNKNOWN, GfxAllSubresources, GfxTextureDimension::TextureCube));
        pass_desc.addItem(GfxBindingSetItem::Sampler(9, m_sampler.Get()));

        auto binding_set = m_device->createBindingSet(pass_desc, m_mesh_pass_binding_layout.Get());
        if (!binding_set) {
            DO_ERROR("BaselineRenderer: failed to create mesh pass binding set");
            return false;
        }
        m_mesh_pass_binding_set = binding_set;
        return true;
    }

    cutie::BindingSetHandle BaselineRenderer::resolveMeshMaterialBindingSet(const MaterialInstance* material_instance) {
        if (!material_instance || material_instance->desc.name.empty()) {
            return {};
        }
        if (const auto it = m_mesh_material_binding_sets.find(material_instance->desc.name);
            it != m_mesh_material_binding_sets.end()) {
            return it->second;
        }
        if (material_instance->textures.empty() || !material_instance->sampler) {
            return {};
        }
        const auto& base_color = material_instance->textures[0];
        const auto& metallic_rough = material_instance->textures.size() > 1
            ? material_instance->textures[1]
            : material_instance->textures[0];
        auto binding_set = m_device->createBindingSet(
            GfxBindingSetDesc()
                .addItem(GfxBindingSetItem::Sampler(shader_bindings::kMaterialBindingSampler, material_instance->sampler.Get()))
                .addItem(GfxBindingSetItem::Texture_SRV(shader_bindings::kMaterialBindingBaseColor, base_color->getRHIHandle()))
                .addItem(GfxBindingSetItem::Texture_SRV(shader_bindings::kMaterialBindingMetallicRough, metallic_rough->getRHIHandle())),
            m_mesh_material_binding_layout.Get());
        if (!binding_set) {
            return {};
        }
        m_mesh_material_binding_sets.emplace(material_instance->desc.name, binding_set);
        return binding_set;
    }

    void BaselineRenderer::renderMeshes(RenderView& view, RenderScene& scene,
                                        const GfxViewportState& viewport_state) {
        if (!m_device || !m_command_list || !m_shared_render_service || !m_mesh_pipeline) {
            return;
        }
        static UInt64 s_mesh_debug_frame = 0;
        const Bool mesh_debug = ((s_mesh_debug_frame++) % 120) == 0;
        const auto* mesh_ext = view.getExtension<MeshViewExtension>();
        if (!mesh_ext || mesh_ext->instance_scene_data.empty()) {
            if (mesh_debug) {
                DO_INFO("BaselineRenderer: mesh draw skipped (ext={} primitives={} instances={})",
                    mesh_ext != nullptr,
                    mesh_ext ? mesh_ext->visible_primitives.size() : 0,
                    mesh_ext ? mesh_ext->instance_scene_data.size() : 0);
            }
            return;
        }
        const auto& primitive_indices = mesh_ext->getMeshPassPrimitiveIndices(MeshPassType::Opaque);
        if (primitive_indices.empty()) {
            if (mesh_debug) {
                DO_INFO("BaselineRenderer: mesh draw skipped (no opaque indices, primitives={})",
                    mesh_ext->visible_primitives.size());
            }
            return;
        }
        if (!ensureMeshInstanceCapacity(static_cast<UInt32>(mesh_ext->instance_scene_data.size()))) {
            return;
        }

        m_command_list->setBufferState(m_mesh_instance_buffer.Get(), cutie::ResourceStates::CopyDest);
        m_command_list->commitBarriers();
        m_command_list->writeBuffer(m_mesh_instance_buffer.Get(), mesh_ext->instance_scene_data.data(),
            mesh_ext->instance_scene_data.size() * sizeof(InstanceSceneData));

        const GlobalMeshShaderData global_data{mesh_ext->frame_time_data};
        m_command_list->writeBuffer(m_mesh_global_cb.Get(), &global_data, sizeof(global_data));
        const ViewMeshShaderData view_data{Math::FlipClipSpaceY(view.getViewProjectionMatrix())};
        m_command_list->writeBuffer(m_mesh_view_cb.Get(), &view_data, sizeof(view_data));

        LitPassConstantBuffer pass_cb{};
        BuildLitPassConstantBuffer(pass_cb, scene, rendering_pipeline_utils::ExtractCameraPosition(view));
        m_command_list->writeBuffer(m_mesh_pass_cb.Get(), &pass_cb, sizeof(pass_cb));
        m_command_list->setBufferState(m_mesh_pass_cb.Get(), cutie::ResourceStates::ConstantBuffer);
        if (!updateMeshPassBindingSet(scene)) {
            return;
        }

        m_command_list->setBufferState(m_mesh_instance_buffer.Get(), cutie::ResourceStates::VertexBuffer);
        m_command_list->commitBarriers();

        DynamicArray<UInt32> instance_prefix(mesh_ext->visible_primitives.size() + 1, 0);
        for (Size_t i = 0; i < mesh_ext->visible_primitives.size(); ++i) {
            const auto* primitive = mesh_ext->visible_primitives[i];
            instance_prefix[i + 1] = instance_prefix[i] + (primitive ? primitive->getInstanceCount() : 0);
        }

        UInt32 mesh_draw_count = 0;
        UInt32 mesh_skip_buffer = 0;
        UInt32 mesh_skip_material = 0;
        UInt32 mesh_skip_batch = 0;

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
                if (!batch.isValid() || !batch.isRelevant(MeshPassType::Opaque) || batch.elements.empty()) {
                    mesh_skip_batch++;
                    continue;
                }
                const auto& element = batch.elements[0];
                if (!element.isValid() || !element.vertex_buffer || !element.index_buffer ||
                    !element.vertex_buffer->isGpuReady() || !element.index_buffer->isGpuReady()) {
                    mesh_skip_buffer++;
                    continue;
                }
                const auto* material_instance = batch.material_instance;
                if (!material_instance) {
                    mesh_skip_material++;
                    continue;
                }
                const cutie::BindingSetHandle material_binding_set = material_instance->texture_binding_set
                    ? material_instance->texture_binding_set->getRHIHandle()
                    : resolveMeshMaterialBindingSet(material_instance);
                if (!material_binding_set) {
                    if (!m_material_warning_logged) {
                        m_material_warning_logged = true;
                        DO_WARN("BaselineRenderer: material has no resolvable texture binding set, primitive skipped");
                    }
                    mesh_skip_material++;
                    continue;
                }

                PrimitiveMeshDrawShaderData shader_data{};
                shader_data.draw_data.x = material_instance->texture_descriptor_indices.empty()
                    ? -1
                    : material_instance->texture_descriptor_indices[0];
                shader_data.draw_data.y = material_instance->texture_descriptor_indices.size() > 1
                    ? material_instance->texture_descriptor_indices[1]
                    : -1;
                shader_data.draw_data.z = material_instance->texture_descriptor_indices.size() > 1 ? 1 : 0;
                m_command_list->writeBuffer(m_mesh_primitive_cb.Get(), &shader_data, sizeof(shader_data));

                cutie::GraphicsState graphics_state;
                graphics_state.setPipeline(m_mesh_pipeline.Get());
                graphics_state.setFramebuffer(m_scene_framebuffer->getRHI());
                graphics_state.setViewport(viewport_state);
                graphics_state.addBindingSet(m_mesh_global_binding_set.Get());
                graphics_state.addBindingSet(m_mesh_view_binding_set.Get());
                graphics_state.addBindingSet(material_binding_set);
                graphics_state.addBindingSet(m_mesh_pass_binding_set.Get());
                graphics_state.addBindingSet(m_mesh_primitive_binding_set.Get());
                graphics_state.addVertexBuffer(
                    cutie::VertexBufferBinding().setBuffer(element.vertex_buffer->getRHI()).setSlot(0).setOffset(0));
                graphics_state.addVertexBuffer(
                    cutie::VertexBufferBinding().setBuffer(m_mesh_instance_buffer.Get()).setSlot(1).setOffset(instance_offset));
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
                mesh_draw_count++;
            }
        }

        if (mesh_debug) {
            DO_INFO("BaselineRenderer: mesh draw primitives={} instances={} opaque={} draws={} skip_batch={} skip_buffer={} skip_material={}",
                mesh_ext->visible_primitives.size(),
                mesh_ext->instance_scene_data.size(),
                primitive_indices.size(),
                mesh_draw_count, mesh_skip_batch, mesh_skip_buffer, mesh_skip_material);
        }
    }

    void BaselineRenderer::render(GfxContext& gfx, UInt32 swapchain_image_index,
                                  RenderViewFamily& view_family, RenderScene& scene) {
        if (!m_device || !m_command_list) {
            DO_ERROR("BaselineRenderer: render skipped, device or command list is null");
            return;
        }

        const auto framebuffer = gfx.getSwapchainFramebuffer(swapchain_image_index);
        if (!framebuffer || !framebuffer->isGpuReady()) {
            DO_ERROR("BaselineRenderer: swapchain framebuffer is unavailable, index={}", swapchain_image_index);
            return;
        }

        const auto& textures = gfx.getSwapchainTextures();
        cutie::ITexture* color_attachment = nullptr;
        if (swapchain_image_index < textures.size() && textures[swapchain_image_index] &&
            textures[swapchain_image_index]->isGpuReady()) {
            color_attachment = textures[swapchain_image_index]->getRHI();
        }

        const auto extent = gfx.getSwapchainExtent2D();
        if (!ensureRenderTarget(extent)) {
            return;
        }
        const auto& framebuffer_info = m_scene_framebuffer->getFramebufferInfo().getRHI();
        ensureSpritePipeline(framebuffer_info);
        ensureMeshPipeline(framebuffer_info);
        ensurePresentPipeline(framebuffer->getFramebufferInfo().getRHI());

        m_command_list->open();
        m_command_list->setTextureState(m_scene_color->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
        m_command_list->setTextureState(m_scene_depth->getRHI(), cutie::AllSubresources, cutie::ResourceStates::DepthWrite);
        m_command_list->commitBarriers();

        m_command_list->clearTextureFloat(m_scene_color->getRHI(), cutie::AllSubresources, cutie::Color(0.08f, 0.09f, 0.12f, 1.0f));
        m_command_list->clearDepthStencilTexture(m_scene_depth->getRHI(), cutie::AllSubresources, true, 1.0f, false, 0);

        for (auto& view : view_family.getViews()) {
            const auto viewport_state = rendering_pipeline_utils::BuildViewportState(view, extent);
            setupMeshExtension(view, view_family);
            renderMeshes(view, scene, viewport_state);
            renderSprites(view, scene, m_scene_framebuffer->getRHI(), viewport_state);
        }

        m_command_list->setTextureState(m_scene_color->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
        if (color_attachment) {
            m_command_list->setTextureState(color_attachment, cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
        }
        m_command_list->commitBarriers();

        if (color_attachment && m_present_pipeline) {
            auto present_binding_set = m_device->createBindingSet(
                GfxBindingSetDesc()
                    .addItem(GfxBindingSetItem::Texture_SRV(1, m_scene_color->getRHIHandle().Get()))
                    .addItem(GfxBindingSetItem::Sampler(9, m_sampler.Get())),
                m_present_binding_layout.Get());
            if (present_binding_set) {
                GfxViewportState present_viewport;
                present_viewport.addViewportAndScissorRect(GfxViewport(
                    0.0f, static_cast<Float>(extent.x),
                    0.0f, static_cast<Float>(extent.y),
                    0.0f, 1.0f));
                cutie::GraphicsState present_state;
                present_state.setPipeline(m_present_pipeline.Get());
                present_state.setFramebuffer(framebuffer->getRHI());
                present_state.setViewport(present_viewport);
                present_state.addBindingSet(present_binding_set.Get());
                m_command_list->setGraphicsState(present_state);
                m_command_list->draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
            }
        }

        if (color_attachment) {
            m_command_list->setTextureState(color_attachment, cutie::AllSubresources, cutie::ResourceStates::Present);
        }
        m_command_list->commitBarriers();
        m_command_list->close();

        m_device->executeCommandList(m_command_list.Get());
        m_device->runGarbageCollection();

        if (((m_frame_counter++) % 120) == 0) {
#if defined(_WIN32)
            PROCESS_MEMORY_COUNTERS_EX pmc{};
            K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc));
            DO_INFO("BaselineRenderer: frame={} private_commit={:.1f} MB",
                m_frame_counter - 1,
                static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0));
#else
            DO_INFO("BaselineRenderer: frame={}", m_frame_counter - 1);
#endif
        }
    }

} // namespace dodoe
