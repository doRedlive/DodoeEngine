// do@Redlive

#include "imgui_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_imgui_pass.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/shared_render_service.h"
#include "runtime/function/render/render_service/binding_set_cache.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/graphics/draw_command_list.h"

#ifdef DODOE_DEBUG_ENABLED
#include "imgui/imgui.h"
#endif

namespace dodoe {

	void ImGuiFeature::initialize(SharedRenderService& resources) {
#ifdef DODOE_DEBUG_ENABLED
	    ImGuiIO& io = ImGui::GetIO();
	    unsigned char* pixels = nullptr;
	    int width = 0;
	    int height = 0;
	    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	    if (pixels && width > 0 && height > 0) {
	        GfxTextureDesc font_desc = GfxTextureDesc()
	            .setDimension(GfxTextureDimension::Texture2D)
	            .setWidth(width)
	            .setHeight(height)
	            .setFormat(GfxFormat::RGBA8_UNORM)
	            .setMipLevels(1)
	            .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
	            .setDebugName("ImGui Font Texture");
	        m_font_texture = GDrawCommandList.createTexture(font_desc, pixels, static_cast<Size_t>(width) * height * 4u);
	        io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(m_font_texture.get()));
	    }
#endif

	    auto* cache = resources.getBindingLayoutCache();
	    m_binding_layout = cache->getOrCreate(
	        GfxBindingLayoutDesc()
	            .setVisibility(GfxShaderType::All)
	            .addItem(GfxBindingLayoutItem::PushConstants(0, 16))
	            .addItem(GfxBindingLayoutItem::Texture_SRV(0))
	            .addItem(GfxBindingLayoutItem::Sampler(0)));

	    if (m_font_texture && resources.getBindingSetCache()) {
	        const auto layout_generation = cache->getLayoutGeneration(m_binding_layout);
	        m_font_binding_set = resources.getBindingSetCache()->getOrCreate(
	            GfxBindingSetDesc()
	                .addItem(GfxBindingSetItem::Texture_SRV(0, m_font_texture->getRHIHandle().Get()))
	                .addItem(GfxBindingSetItem::Sampler(0, GlobalSamplers::screen().Get())),
	            m_binding_layout,
	            layout_generation);
	    }

#ifdef DODOE_DEBUG_ENABLED
	    if (auto* input_layout_cache = resources.getInputLayoutCache()) {
	        const DynamicArray<GfxVertexAttributeDesc> attributes = {
	            GfxVertexAttributeDesc().setName("a_Position").setFormat(GfxFormat::RG32_FLOAT).setOffset(0).setElementStride(sizeof(ImDrawVert)),
	            GfxVertexAttributeDesc().setName("a_UV").setFormat(GfxFormat::RG32_FLOAT).setOffset(sizeof(ImVec2)).setElementStride(sizeof(ImDrawVert)),
	            GfxVertexAttributeDesc().setName("a_Color").setFormat(GfxFormat::RGBA8_UNORM).setOffset(sizeof(ImVec2) * 2).setElementStride(sizeof(ImDrawVert)),
	        };
	        m_input_layout = input_layout_cache->getOrCreate(
	            attributes, resources.getShaderLibrary()->getImGuiVertexShader());
	    }
#endif
	}

	void ImGuiFeature::shutdown() {
#ifdef DODOE_DEBUG_ENABLED
	    if (ImGui::GetCurrentContext()) {
	        ImGui::GetIO().Fonts->SetTexID(ImTextureID_Invalid);
	    }
#endif
	    m_font_texture.reset();
	    m_font_binding_set.reset();
	    m_input_layout.reset();
	    m_binding_layout.reset();
	}

	void ImGuiFeature::exportResources(ResourceRegistry& registry,
	                                   const RenderView& view) {
	    if (m_font_texture) {
	        registry.registerTexture("ImGuiFontTexture", m_font_texture,
	                                 GfxFormat::RGBA8_UNORM,
	                                 RenderTargetScalePolicy::Fixed);
	    }
	}

	void ImGuiFeature::collectPasses(PassCollector& collector) {
	    collector.addPass<ImGuiPass>(m_binding_layout, m_font_binding_set, m_font_texture, m_input_layout);
	}

} // namespace dodoe
