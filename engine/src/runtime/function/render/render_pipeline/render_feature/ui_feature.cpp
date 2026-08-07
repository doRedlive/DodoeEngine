// do@Redlive

#include "ui_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_ui_pass.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_scene/sprite_scene_info.h"
#include "runtime/function/ui/ui_types.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/render_settings.h"

namespace dodoe {

	void UIFeature::initialize(SharedRenderService& resources) {
	    auto* cache = resources.getBindingLayoutCache();

	    m_view_binding_layout = cache->getOrCreate(
	        GfxBindingLayoutDesc().setVisibility(GfxShaderType::Vertex | GfxShaderType::Pixel)
	            .setRegisterSpaceIsDescriptorSet(true)
	            .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::View))
	            .addItem(GfxBindingLayoutItem::ConstantBuffer(0)));

	    m_bindless_binding_layout = cache->getOrCreate(
	        GfxBindingLayoutDesc().setVisibility(GfxShaderType::Vertex | GfxShaderType::Pixel)
	            .setRegisterSpaceIsDescriptorSet(true)
	            .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Material))
	            .addItem(GfxBindingLayoutItem::Sampler(1)));

	    m_material_binding_layout = cache->getOrCreate(
	        GfxBindingLayoutDesc().setVisibility(GfxShaderType::Vertex | GfxShaderType::Pixel)
	            .setRegisterSpaceIsDescriptorSet(true)
	            .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Material))
	            .addItem(GfxBindingLayoutItem::Texture_SRV(2))
	            .addItem(GfxBindingLayoutItem::Sampler(1)));

	    if (auto* input_layout_cache = resources.getInputLayoutCache()) {
	        const DynamicArray<GfxVertexAttributeDesc> attributes = {
	            GfxVertexAttributeDesc().setName("POSITION").setFormat(GfxFormat::RGB32_FLOAT).setOffset(0).setElementStride(sizeof(QuadVertex)),
	            GfxVertexAttributeDesc().setName("TEXCOORD").setFormat(GfxFormat::RG32_FLOAT).setOffset(sizeof(Vector3f)).setElementStride(sizeof(QuadVertex)),
	            GfxVertexAttributeDesc().setName("COLOR").setFormat(GfxFormat::RGBA8_UNORM).setOffset(sizeof(Vector3f) + sizeof(Vector2f)).setElementStride(sizeof(QuadVertex)),
	            GfxVertexAttributeDesc().setName("TEXINDEX").setFormat(GfxFormat::R32_UINT).setOffset(sizeof(Vector3f) + sizeof(Vector2f) + sizeof(UInt32)).setElementStride(sizeof(QuadVertex)),
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
	    m_input_layout = nullptr;
	    m_material_binding_layout = nullptr;
	    m_bindless_binding_layout = nullptr;
	    m_view_binding_layout = nullptr;
	}

	void UIFeature::collectPasses(PassCollector& collector) {
	    collector.addPass<UIPass>(m_view_binding_layout, m_bindless_binding_layout, m_material_binding_layout, m_input_layout);
	}

} // namespace dodoe
