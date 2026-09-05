// do@Redlive

#include "baseline_sprite_pass.h"

#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/sprite_view_extension.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_scene/sprite_scene_info.h"
#include "runtime/function/render/texture/texture_manager.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/function/render/gpu_driven/gpu_scene.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    namespace {
        constexpr UInt32 kInitialInstanceCapacity = 256;
    }

    Bool BaselineSpritePass::initialize(const BaselinePassContext& context) {
        m_device = context.device;
        m_command_list = context.command_list;
        m_shader_library = context.shader_library;
        m_shared_render_service = context.shared_render_service;
        m_sampler = context.sampler;

        if (!m_shared_render_service) {
            DO_INFO("BaselineSpritePass: shared render service unavailable, 2d scene drawing disabled");
            return true;
        }
        if (!m_shader_library) {
            DO_ERROR("BaselineSpritePass: shader library is unavailable for sprite resources");
            return false;
        }

        GfxBindingLayoutDesc cb_desc;
        cb_desc.setVisibility(GfxShaderType::Vertex | GfxShaderType::Pixel)
            .setRegisterSpaceIsDescriptorSet(true)
            .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::View))
            .addItem(GfxBindingLayoutItem::ConstantBuffer(0));
        m_cb_binding_layout = m_device->createBindingLayout(cb_desc);

        GfxBindingLayoutDesc material_desc;
        material_desc.setVisibility(GfxShaderType::Vertex | GfxShaderType::Pixel)
            .setRegisterSpaceIsDescriptorSet(true)
            .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Material))
            .addItem(GfxBindingLayoutItem::Texture_SRV(2))
            .addItem(GfxBindingLayoutItem::Sampler(1));
        m_material_binding_layout = m_device->createBindingLayout(material_desc);

        auto* input_layout_cache = m_shared_render_service->getInputLayoutCache();
        DO_ASSERT(input_layout_cache != nullptr, "BaselineSpritePass: input layout cache is unavailable");
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
        m_input_layout = input_layout_cache->getOrCreate(attributes, m_shader_library->getSpriteVertexShader());

        GfxBufferDesc vp_desc;
        vp_desc.setByteSize(sizeof(Matrix4f))
            .setIsConstantBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
            .setDebugName("BaselineSpriteVP");
        m_vp_buffer = m_device->createBuffer(vp_desc);
        return true;
    }

    void BaselineSpritePass::shutdown() {
        m_pipeline = nullptr;
        m_cb_binding_layout = nullptr;
        m_material_binding_layout = nullptr;
        m_input_layout = nullptr;
        m_instance_buffer = nullptr;
        m_vp_buffer = nullptr;
        m_instance_capacity = 0;
        m_sampler = nullptr;
        m_shader_library = nullptr;
        m_shared_render_service = nullptr;
        m_command_list = nullptr;
        m_device = nullptr;
    }

    void BaselineSpritePass::ensurePipeline(const cutie::FramebufferInfo& framebuffer_info) {
        if (m_pipeline) {
            return;
        }
        if (!m_shader_library || !m_shared_render_service) {
            return;
        }
        const auto vertex_shader = m_shader_library->getSpriteVertexShader();
        const auto pixel_shader = m_shader_library->getSpritePixelShaderTraditional();
        if (!vertex_shader || !pixel_shader) {
            DO_ERROR("BaselineSpritePass: sprite shaders are not loaded");
            return;
        }

        GfxGraphicsPipelineDesc pipeline_desc;
        pipeline_desc.setPrimType(GfxPrimitiveType::TriangleList);
        pipeline_desc.setVertexShader(vertex_shader.Get());
        pipeline_desc.setPixelShader(pixel_shader.Get());
        pipeline_desc.setInputLayout(m_input_layout.Get());
        pipeline_desc.addBindingLayout(m_cb_binding_layout.Get());
        pipeline_desc.addBindingLayout(m_material_binding_layout.Get());

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

        m_pipeline = m_device->createGraphicsPipeline(pipeline_desc, framebuffer_info);
        DO_INFO("BaselineSpritePass: render pipeline created");
    }

    void BaselineSpritePass::ensureInstanceCapacity(UInt32 instance_count) {
        if (instance_count <= m_instance_capacity) {
            return;
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
        m_instance_buffer = m_device->createBuffer(desc);
        m_instance_capacity = capacity;
    }

    void BaselineSpritePass::collectInstances(RenderView& view, RenderScene& scene,
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

    void BaselineSpritePass::render(RenderView& view, RenderScene& scene,
                                    cutie::IFramebuffer* framebuffer, const GfxViewportState& viewport_state) {
        if (!m_shared_render_service || !m_pipeline) {
            return;
        }

        DynamicArray<SpriteInstance> instances;
        collectInstances(view, scene, instances);
        if (instances.empty()) {
            return;
        }
        ensureInstanceCapacity(static_cast<UInt32>(instances.size()));
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
                m_cb_binding_layout.Get());
            auto material_binding_set = m_device->createBindingSet(
                GfxBindingSetDesc()
                    .addItem(GfxBindingSetItem::Texture_SRV(2, texture->getRHIHandle().Get()))
                    .addItem(GfxBindingSetItem::Sampler(1, m_sampler.Get())),
                m_material_binding_layout.Get());

            cutie::GraphicsState graphics_state;
            graphics_state.setPipeline(m_pipeline.Get());
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

} // namespace dodoe
