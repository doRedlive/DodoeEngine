// do@Redlive

#include "imgui_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_imgui_pass.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/binding_set_cache.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/graphics/draw_command_list.h"

#ifdef DODOE_DEBUG_ENABLED
#include "imgui/imgui.h"
#include "runtime/function/ui/imgui/imgui_builder.h"
#include "runtime/function/ui/imgui/imgui_draw_renderer.h"
#include "runtime/function/render/render_settings.h"
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

	    auto* gfx = resources.getGfxContext();
	    m_imgui_cb = create_ref<GfxBuffer>(
	        GfxBufferDesc()
	            .setByteSize(16)
	            .setIsConstantBuffer(true)
	            .enableAutomaticStateTracking(GfxResourceStates::ConstantBuffer)
	            .setDebugName("ImGuiViewportCB"));
	    m_imgui_cb->initializeRHI(gfx->getDevice());

	    auto* cache = resources.getBindingLayoutCache();
	    m_binding_layout = cache->getOrCreate(
	        GfxBindingLayoutDesc()
	            .setVisibility(GfxShaderType::All)
	            .setRegisterSpaceIsDescriptorSet(true)
	            .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
	            .addItem(GfxBindingLayoutItem::ConstantBuffer(0))
	            .addItem(GfxBindingLayoutItem::Texture_SRV(1))
	            .addItem(GfxBindingLayoutItem::Sampler(9)));

	    if (m_font_texture && resources.getBindingSetCache()) {
	        const auto layout_generation = cache->getLayoutGeneration(m_binding_layout);
	        m_font_binding_set = resources.getBindingSetCache()->getOrCreate(
	            GfxBindingSetDesc()
	                .addItem(GfxBindingSetItem::ConstantBuffer(0, m_imgui_cb->getRHIHandle().Get()))
	                .addItem(GfxBindingSetItem::Texture_SRV(1, m_font_texture->getRHIHandle().Get()))
	                .addItem(GfxBindingSetItem::Sampler(9, GlobalSamplers::screen().Get())),
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
	    setupViewports(resources);
#endif//DODOE_DEBUG_ENABLED
	}

#ifdef DODOE_DEBUG_ENABLED
	void ImGuiFeature::setupViewports(SharedRenderService& resources) {
	    const auto viewport_api = RenderSettings::GetRenderBackendApiType();
	    const auto viewport_threading = RenderSettings::GetThreadingMode();
	    Bool viewports_supported = viewport_api == RenderBackendApiType::D3D12;
	    if (viewport_api == RenderBackendApiType::Vulkan) {
	        viewports_supported = viewport_threading != ThreadingMode::TripleThread;
	    } else if (viewport_api == RenderBackendApiType::OpenGL) {
	        viewports_supported = viewport_threading == ThreadingMode::SingleThread;
	    }
	    if (!viewports_supported) {
	        DO_WARN("ImGui multi-viewport disabled for this backend/threading combination");
	        return;
	    }

	    auto* gfx = resources.getGfxContext();
	    if (!gfx || !m_binding_layout || !m_input_layout || !m_font_texture) {
	        return;
	    }
	    ImGuiDrawRenderer draw_renderer(m_binding_layout, m_font_binding_set, m_input_layout, m_font_texture, m_imgui_cb);
	    ImGuiBuilder::InstallViewportRenderer(*gfx, std::move(draw_renderer),
	                                          resources.getPipelineStateCache(),
	                                          resources.getShaderLibrary());
	}
#endif//DODOE_DEBUG_ENABLED

	void ImGuiFeature::shutdown() {
#ifdef DODOE_DEBUG_ENABLED
	    if (ImGui::GetCurrentContext()) {
	        ImGui::GetIO().Fonts->SetTexID(ImTextureID_Invalid);
	    }
#endif//DOODE_DEBUG_ENABLED
	    m_font_texture = nullptr;
	    m_font_binding_set = nullptr;
	    m_input_layout = nullptr;
	    m_binding_layout = nullptr;
	    m_imgui_cb.reset();
	}

	void ImGuiFeature::registerGraphImports(RenderGraphImportRegistry& imports,
	                                        const RenderView& view) {
	    if (m_font_texture) {
	        imports.publish<ImGuiFontTextureKey>(m_font_texture);
	    }
	    if (m_imgui_cb) {
	        imports.publish<ImGuiConstantBufferKey>(m_imgui_cb);
	    }
	}

	void ImGuiFeature::collectPasses(PassCollector& collector) {
#ifdef DODOE_DEBUG_ENABLED
	    collector.addPass<ImGuiPass>(m_binding_layout, m_font_binding_set, m_input_layout, m_font_texture, m_imgui_cb);
#endif//DODOE_DEBUG_ENABLED
	}

} // namespace dodoe
