// do@Redlive

#include "sprite_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_sprite_pass.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_scene/sprite_scene_info.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

	void SpriteFeature::initialize(SharedRenderService& resources) {
	    auto* cache = resources.getBindingLayoutCache();

	    m_cb_binding_layout = cache->getOrCreate(
	        GfxBindingLayoutDesc().setVisibility(GfxShaderType::Vertex | GfxShaderType::Pixel)
	            .setRegisterSpaceIsDescriptorSet(true)
	            .addItem(GfxBindingLayoutItem::ConstantBuffer(0)));

	    m_sampler_binding_layout = cache->getOrCreate(
	        GfxBindingLayoutDesc().setVisibility(GfxShaderType::Vertex | GfxShaderType::Pixel)
	            .setRegisterSpaceIsDescriptorSet(true)
	            .setRegisterSpace(1)
	            .addItem(GfxBindingLayoutItem::Sampler(0)));

	    m_texture_binding_layout = cache->getOrCreate(
	        GfxBindingLayoutDesc().setVisibility(GfxShaderType::Vertex | GfxShaderType::Pixel)
	            .setRegisterSpaceIsDescriptorSet(true)
	            .setRegisterSpace(2)
	            .addItem(GfxBindingLayoutItem::Texture_SRV(0)));

	    if (auto* input_layout_cache = resources.getInputLayoutCache()) {
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
	        m_input_layout = input_layout_cache->getOrCreate(
	            attributes, resources.getShaderLibrary()->getSpriteVertexShader());
	    }
	}

	void SpriteFeature::shutdown() {
	    m_input_layout = nullptr;
	    m_texture_binding_layout = nullptr;
	    m_sampler_binding_layout = nullptr;
	    m_cb_binding_layout = nullptr;
	}

	void SpriteFeature::collectPasses(PassCollector& collector) {
	    collector.addPass<SpritePass>(m_cb_binding_layout, m_sampler_binding_layout, m_texture_binding_layout, m_input_layout);
	}

} // namespace dodoe
