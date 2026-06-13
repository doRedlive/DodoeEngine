//
// Created by GreenMuffin on 2026/3/6.
// Knight!
//

#include "render_system.h"

#include "render_resource.h"
#include "render_settings.h"

#include "passes/color_grading_pass.h"
#include "passes/combine_pass.h"
#include "passes/deferred_light_pass.h"
#include "passes/directional_light_shadow_pass.h"
#include "passes/fxaa_pass.h"
#include "passes/main_camera_pass.h"
#include "passes/point_light_shadow_pass.h"
#include "passes/skybox_pass.h"
#include "passes/sprite_pass.h"
#include "passes/tone_mapping_pass.h"
#ifdef DODOE_EDITOR
#include "passes/imgui_pass.h"
#include "passes/pick_pass.h"
#endif

#include "framework/texture_manager.h"

#include "runtime/core/utils/util.h"

namespace dodoe {

    bool RenderSystem::initialize(const RenderSystemCreateInfo& info) {
        m_window_manager = info.window_manager;

        auto window = m_window_manager->getWindow();
        auto backend_api = RenderSettings::GetRenderBackendApiType();

        m_viewport_manager = ViewportManager::Create({window});
        const bool enable_validation =
#ifdef DO_DEBUG
            true;
#else
            false;
#endif
        m_rhi = RhiContext::Create({window->getNativeWindow(), backend_api, enable_validation, window->isHostMode() ? window->getNativeHandle() : nullptr});
        const auto camera_type = RenderSettings::GetRenderingPipelineType() == RenderingPipelineType::Only2D
            ? CameraType::Orthographic : CameraType::Perspective;
        m_camera = Camera::Create({camera_type, m_viewport_manager->getLogicalSize(), m_viewport_manager->getWindowSize()});
        m_descriptor_table = DescriptorTableManager::Create({m_rhi.get()});
        m_texture_manager = TextureManager::Create({m_rhi.get(), m_descriptor_table.get()});

        g_RenderResource->initilize(m_rhi->getDevice());

        m_render_graph = RenderGraph::Create({m_rhi.get(), m_camera.get(), m_descriptor_table.get()});

        buildRenderGraph();
        m_render_graph->compile();

        return m_camera && m_descriptor_table && m_texture_manager && m_render_graph;
    }

    void RenderSystem::shutdown() {
        if (m_rhi && m_rhi->getDevice()) {
            m_rhi->getDevice()->waitForIdle();
        }

        RenderGraph::Destroy(m_render_graph);
        MeshPassProcessor::Cleanup();
        Camera::Destroy(m_camera);
        g_RenderResource->shutdown();
        TextureManager::Destroy(m_texture_manager);
        DescriptorTableManager::Destroy(m_descriptor_table);
        if (m_rhi && m_rhi->getDevice()) {
            m_rhi->getDevice()->waitForIdle();
            m_rhi->getDevice()->runGarbageCollection();
        }
        RhiContext::Destroy(m_rhi);
    }

    void RenderSystem::prepare() {
        m_viewport_manager->update();
        if (m_viewport_manager->isViewportDirty()) [[unlikely]] {
            m_camera->setViewportSize(m_viewport_manager->getLogicalSize(), m_viewport_manager->getWindowSize());
            m_render_graph->onViewportResize(m_viewport_manager->viewport());
        }
        if (m_viewport_manager->isWindowDirty()) [[unlikely]] {
            if (!m_rhi->recreateSwapchain()) {
                return;
            }
            m_rhi->getDevice()->runGarbageCollection();
            m_camera->setViewportSize(m_viewport_manager->getLogicalSize(), m_viewport_manager->getWindowSize());
            m_render_graph->onWindowResize(m_viewport_manager->getPixelSize());
        }
        m_viewport_manager->clearDirtyFlags();

        swapLogicRenderContext();
    }

    void RenderSystem::present() {
        uint32_t image_index = 0;
        if (!m_rhi->acquireNextSwapchainImage(image_index)) {
            return;
        }

        m_render_graph->execute(image_index);

        if (m_rhi->presentSwapchainImage(image_index)) {
            m_rhi->getDevice()->runGarbageCollection();
        }
    }

    void RenderSystem::swapLogicRenderContext() {
        g_RenderResource->swapLogicRenderContext();
    }

    void RenderSystem::buildRenderGraph() {
        if (!m_render_graph) {
            return;
        }

        auto pipeline = RenderSettings::GetRenderingPipelineType();

        switch (pipeline) {
        case RenderingPipelineType::Only2D:
            buildOnly2DPipeline();
            break;
        case RenderingPipelineType::Forward:
        case RenderingPipelineType::ForwardPlus:
        case RenderingPipelineType::DeferredPlus:
            // TODO: implement additional pipeline types
        case RenderingPipelineType::Deferred:
        default:
            buildDeferredPipeline();
            break;
        }
    }

    void RenderSystem::buildDeferredPipeline() {
        TextureResourceDesc main_camera_color_desc{};
        main_camera_color_desc.format = rhi::Format::RGBA8_UNORM;
        main_camera_color_desc.viewport_relative = true;
        main_camera_color_desc.shader_resource = true;
        main_camera_color_desc.render_target = true;
        main_camera_color_desc.debug_name = "MainCameraPass Scene Target";

        TextureResourceDesc main_camera_hdr_color_desc{};
        main_camera_hdr_color_desc.format = rhi::Format::RGBA16_FLOAT;
        main_camera_hdr_color_desc.viewport_relative = true;
        main_camera_hdr_color_desc.shader_resource = true;
        main_camera_hdr_color_desc.render_target = true;
        main_camera_hdr_color_desc.debug_name = "MainCameraPass HDR Color Target";

        TextureResourceDesc main_camera_tonemapped_color_desc{};
        main_camera_tonemapped_color_desc.format = rhi::Format::RGBA8_UNORM;
        main_camera_tonemapped_color_desc.viewport_relative = true;
        main_camera_tonemapped_color_desc.shader_resource = true;
        main_camera_tonemapped_color_desc.render_target = true;
        main_camera_tonemapped_color_desc.debug_name = "MainCameraPass ToneMapped Color Target";

        TextureResourceDesc main_camera_fxaa_color_desc{};
        main_camera_fxaa_color_desc.format = rhi::Format::RGBA8_UNORM;
        main_camera_fxaa_color_desc.viewport_relative = true;
        main_camera_fxaa_color_desc.shader_resource = true;
        main_camera_fxaa_color_desc.render_target = true;
        main_camera_fxaa_color_desc.debug_name = "MainCameraPass FXAA Color Target";

        TextureResourceDesc main_camera_albedo_desc{};
        main_camera_albedo_desc.format = rhi::Format::RGBA8_UNORM;
        main_camera_albedo_desc.viewport_relative = true;
        main_camera_albedo_desc.shader_resource = true;
        main_camera_albedo_desc.render_target = true;
        main_camera_albedo_desc.debug_name = "MainCameraPass Albedo Target";

        TextureResourceDesc main_camera_normal_desc{};
        main_camera_normal_desc.format = rhi::Format::RGBA16_FLOAT;
        main_camera_normal_desc.viewport_relative = true;
        main_camera_normal_desc.shader_resource = true;
        main_camera_normal_desc.render_target = true;
        main_camera_normal_desc.debug_name = "MainCameraPass Normal Target";

        TextureResourceDesc main_camera_position_desc{};
        main_camera_position_desc.format = rhi::Format::RGBA32_FLOAT;
        main_camera_position_desc.viewport_relative = true;
        main_camera_position_desc.shader_resource = true;
        main_camera_position_desc.render_target = true;
        main_camera_position_desc.debug_name = "MainCameraPass Position Target";

        TextureResourceDesc main_camera_material_desc{};
        main_camera_material_desc.format = rhi::Format::RGBA8_UNORM;
        main_camera_material_desc.viewport_relative = true;
        main_camera_material_desc.shader_resource = true;
        main_camera_material_desc.render_target = true;
        main_camera_material_desc.debug_name = "MainCameraPass Material Target";

        TextureResourceDesc main_camera_depth_desc{};
        main_camera_depth_desc.format = rhi::Format::D32;
        main_camera_depth_desc.viewport_relative = true;
        main_camera_depth_desc.render_target = true;
        main_camera_depth_desc.depth_stencil = true;
        main_camera_depth_desc.shader_resource = true;
        main_camera_depth_desc.debug_name = "MainCameraPass Depth Target";

        TextureResourceDesc directional_shadow_desc{};
        directional_shadow_desc.format = rhi::Format::D32;
        directional_shadow_desc.viewport_relative = true;
        directional_shadow_desc.render_target = true;
        directional_shadow_desc.depth_stencil = true;
        directional_shadow_desc.shader_resource = true;
        directional_shadow_desc.debug_name = "DirectionalLightShadowPass Depth Target";

#ifdef DODOE_EDITOR
        TextureResourceDesc pick_color_desc{};
        pick_color_desc.format = rhi::Format::R32_UINT;
        pick_color_desc.viewport_relative = true;
        pick_color_desc.shader_resource = true;
        pick_color_desc.render_target = true;
        pick_color_desc.debug_name = "PickPass Target";

        TextureResourceDesc pick_depth_desc{};
        pick_depth_desc.format = rhi::Format::D32;
        pick_depth_desc.viewport_relative = true;
        pick_depth_desc.render_target = true;
        pick_depth_desc.depth_stencil = true;
        pick_depth_desc.shader_resource = true;
        pick_depth_desc.debug_name = "PickPass Depth Target";

        TextureResourceDesc imgui_color_desc{};
        imgui_color_desc.format = rhi::Format::RGBA8_UNORM;
        imgui_color_desc.backbuffer_relative = true;
        imgui_color_desc.shader_resource = true;
        imgui_color_desc.render_target = true;
        imgui_color_desc.debug_name = "ImGuiPass Color Target";
#endif

        m_render_graph->addPass("MainCameraPass", create_ref<MainCameraPass>(m_rhi.get(), m_descriptor_table.get()))
            .addTextureWrite("MainCameraAlbedo", main_camera_albedo_desc)
            .addTextureWrite("MainCameraNormal", main_camera_normal_desc)
            .addTextureWrite("MainCameraPosition", main_camera_position_desc)
            .addTextureWrite("MainCameraMaterial", main_camera_material_desc)
            .addTextureWrite("MainCameraDepth", main_camera_depth_desc);

#ifdef DODOE_EDITOR
        m_render_graph->addPass("PickPass", create_ref<PickPass>(m_rhi.get()))
            .addTextureWrite("PickColor", pick_color_desc)
            .addTextureWrite("PickDepth", pick_depth_desc);
#endif

        m_render_graph->addPass("DirectionalLightShadowPass", create_ref<DirectionalLightShadowPass>(m_rhi.get()))
            .addTextureWrite("ShadowMap", directional_shadow_desc);

        m_render_graph->addPass("PointLightShadowPass", create_ref<PointLightShadowPass>(m_rhi.get()));

        m_render_graph->addPass("SkyboxPass", create_ref<SkyboxPass>(m_rhi.get()))
            .addTextureRead("MainCameraDepth", main_camera_depth_desc)
            .addTextureWrite("MainCameraHdrColor", main_camera_hdr_color_desc);

        m_render_graph->addPass("DeferredLightPass", create_ref<DeferredLightPass>(m_rhi.get()))
            .addTextureRead("MainCameraAlbedo", main_camera_albedo_desc)
            .addTextureRead("MainCameraNormal", main_camera_normal_desc)
            .addTextureRead("MainCameraPosition", main_camera_position_desc)
            .addTextureRead("MainCameraMaterial", main_camera_material_desc)
            .addTextureRead("ShadowMap", directional_shadow_desc)
            .addTextureWrite("MainCameraHdrColor", main_camera_hdr_color_desc);

        m_render_graph->addPass("ToneMappingPass", create_ref<ToneMappingPass>(m_rhi.get()))
            .addTextureRead("MainCameraHdrColor", main_camera_hdr_color_desc)
            .addTextureWrite("MainCameraToneMappedColor", main_camera_tonemapped_color_desc);

        m_render_graph->addPass("ColorGradingPass", create_ref<ColorGradingPass>(m_rhi.get()))
            .addTextureRead("MainCameraToneMappedColor", main_camera_tonemapped_color_desc)
            .addTextureWrite("MainCameraColor", main_camera_color_desc);

        m_render_graph->addPass("SpritePass", create_ref<SpritePass>(m_rhi.get(), m_descriptor_table.get()))
            .addTextureWrite("MainCameraColor", main_camera_color_desc)
            .addTextureRead("MainCameraDepth", main_camera_depth_desc);

        m_render_graph->addPass("FXAAPass", create_ref<FXAAPass>(m_rhi.get()))
            .addTextureRead("MainCameraColor", main_camera_color_desc)
            .addTextureWrite("MainCameraFxaaColor", main_camera_fxaa_color_desc);

#ifdef DODOE_EDITOR
        m_render_graph->addPass("ImGuiPass", create_ref<ImGuiPass>(m_rhi.get()))
            .addTextureWrite("ImGuiColor", imgui_color_desc);
#endif

        auto& combine_pass = m_render_graph->addPass("CombinePass", create_ref<CombinePass>(m_rhi.get()))
            .addTextureRead("MainCameraFxaaColor", main_camera_fxaa_color_desc);
#ifdef DODOE_EDITOR
        combine_pass.addTextureRead("ImGuiColor", imgui_color_desc);
#endif
    }

    void RenderSystem::buildOnly2DPipeline() {
        TextureResourceDesc main_camera_color_desc{};
        main_camera_color_desc.format = rhi::Format::RGBA8_UNORM;
        main_camera_color_desc.viewport_relative = true;
        main_camera_color_desc.shader_resource = true;
        main_camera_color_desc.render_target = true;
        main_camera_color_desc.debug_name = "SpritePass Scene Target";

        TextureResourceDesc main_camera_depth_desc{};
        main_camera_depth_desc.format = rhi::Format::D32;
        main_camera_depth_desc.viewport_relative = true;
        main_camera_depth_desc.render_target = true;
        main_camera_depth_desc.depth_stencil = true;
        main_camera_depth_desc.shader_resource = true;
        main_camera_depth_desc.debug_name = "SpritePass Depth Target";

#ifdef DODOE_EDITOR
        TextureResourceDesc pick_color_desc{};
        pick_color_desc.format = rhi::Format::R32_UINT;
        pick_color_desc.viewport_relative = true;
        pick_color_desc.shader_resource = true;
        pick_color_desc.render_target = true;
        pick_color_desc.debug_name = "PickPass Target";

        TextureResourceDesc pick_depth_desc{};
        pick_depth_desc.format = rhi::Format::D32;
        pick_depth_desc.viewport_relative = true;
        pick_depth_desc.render_target = true;
        pick_depth_desc.depth_stencil = true;
        pick_depth_desc.shader_resource = true;
        pick_depth_desc.debug_name = "PickPass Depth Target";

        TextureResourceDesc imgui_color_desc{};
        imgui_color_desc.format = rhi::Format::RGBA8_UNORM;
        imgui_color_desc.backbuffer_relative = true;
        imgui_color_desc.shader_resource = true;
        imgui_color_desc.render_target = true;
        imgui_color_desc.debug_name = "ImGuiPass Color Target";
#endif

        auto sprite_pass_2d = create_ref<SpritePass>(m_rhi.get(), m_descriptor_table.get());
        sprite_pass_2d->setClearTargets(true);
        m_render_graph->addPass("SpritePass", std::move(sprite_pass_2d))
            .addTextureWrite("MainCameraColor", main_camera_color_desc)
            .addTextureWrite("MainCameraDepth", main_camera_depth_desc);

#ifdef DODOE_EDITOR
        m_render_graph->addPass("PickPass", create_ref<PickPass>(m_rhi.get()))
            .addTextureWrite("PickColor", pick_color_desc)
            .addTextureWrite("PickDepth", pick_depth_desc);
#endif

#ifdef DODOE_EDITOR
        m_render_graph->addPass("ImGuiPass", create_ref<ImGuiPass>(m_rhi.get()))
            .addTextureWrite("ImGuiColor", imgui_color_desc);
#endif

        auto& combine_pass = m_render_graph->addPass("CombinePass", create_ref<CombinePass>(m_rhi.get()))
            .addTextureRead("MainCameraColor", main_camera_color_desc);
#ifdef DODOE_EDITOR
        combine_pass.addTextureRead("ImGuiColor", imgui_color_desc);
#endif
    }

} // dodoe
