// do@Redlive

#include "ui_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_ui_pass.h"
#include "runtime/function/render/shared_render_service.h"
#include "runtime/function/render/render_scene/sprite_scene_info.h"  // QuadVertex
#include "runtime/function/ui/ui_types.h"                             // UIInstance
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/render_settings.h"

namespace dodoe {

	void UIFeature::initialize(SharedRenderService& resources) {
	    auto* cache = resources.getBindingLayoutCache();

	    // Binding layout for bindless path: CB_VS(b0) + Sampler_PS(s0)
	    m_bindless_binding_layout = cache->getOrCreate(
	        GfxBindingLayoutDesc().setVisibility(GfxShaderType::Vertex | GfxShaderType::Pixel)
	            .addItem(GfxBindingLayoutItem::ConstantBuffer(0, GfxShaderType::Vertex))
	            .addItem(GfxBindingLayoutItem::Sampler(0, GfxShaderType::Pixel)));

	    // Binding layout for non-bindless path: CB_VS(b0) + Sampler_PS(s0) + TextureArray_SRV_PS(t0)
	    m_array_binding_layout = cache->getOrCreate(
	        GfxBindingLayoutDesc().setVisibility(GfxShaderType::Vertex | GfxShaderType::Pixel)
	            .addItem(GfxBindingLayoutItem::ConstantBuffer(0, GfxShaderType::Vertex))
	            .addItem(GfxBindingLayoutItem::Sampler(0, GfxShaderType::Pixel))
	            .addItem(GfxBindingLayoutItem::Texture_SRV(0, GfxShaderType::Pixel)));

	    // Input layout: buffer 0 = QuadVertex, buffer 1 = UIInstance (per-instance)
	    if (auto* input_layout_cache = resources.getInputLayoutCache()) {
	        const DynamicArray<GfxVertexAttributeDesc> attributes = {
	            GfxVertexAttributeDesc().setName("POSITION").setFormat(GfxFormat::RGB32_FLOAT).setOffset(0).setElementStride(sizeof(QuadVertex)),
	            GfxVertexAttributeDesc().setName("TEXCOORD").setFormat(GfxFormat::RG32_FLOAT).setOffset(sizeof(Vector3f)).setElementStride(sizeof(QuadVertex)),
	            GfxVertexAttributeDesc().setName("COLOR").setFormat(GfxFormat::RGBA8_UNORM).setOffset(sizeof(Vector3f) + sizeof(Vector2f)).setElementStride(sizeof(QuadVertex)),
	            GfxVertexAttributeDesc().setName("TEXINDEX").setFormat(GfxFormat::R32_UINT).setOffset(sizeof(Vector3f) + sizeof(Vector2f) + sizeof(UInt32)).setElementStride(sizeof(QuadVertex)),
	            // UIInstance per-instance attributes
	            GfxVertexAttributeDesc().setName("TEXCOORD1").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(offsetof(UIInstance, position)).setElementStride(sizeof(UIInstance)).setIsInstanced(true),
	            GfxVertexAttributeDesc().setName("TEXCOORD2").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(offsetof(UIInstance, uv_min)).setElementStride(sizeof(UIInstance)).setIsInstanced(true),
	            GfxVertexAttributeDesc().setName("TEXCOORD3").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(offsetof(UIInstance, color)).setElementStride(sizeof(UIInstance)).setIsInstanced(true),
	            GfxVertexAttributeDesc().setName("TEXCOORD4").setFormat(GfxFormat::RGBA32_FLOAT).setBufferIndex(1).setOffset(offsetof(UIInstance, clip_rect)).setElementStride(sizeof(UIInstance)).setIsInstanced(true),
	        };
	        m_input_layout = input_layout_cache->getOrCreate(
	            attributes, resources.getShaderLibrary()->getUIVertexShader());
	    }
	}

	void UIFeature::shutdown() {
	    m_input_layout.reset();
	    m_array_binding_layout.reset();
	    m_bindless_binding_layout.reset();
	}

	void UIFeature::collectPasses(PassCollector& collector) {
	    collector.addPass<UIPass>(m_bindless_binding_layout, m_array_binding_layout, m_input_layout);
	}

} // namespace dodoe
