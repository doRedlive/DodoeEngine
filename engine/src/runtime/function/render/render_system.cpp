//
// Created by GreenMuffin on 2026/3/6.
// Knight!
//

#include "render_system.h"

#include "render_settings.h"

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
        m_gfx = GfxContext::Create({window->getNativeWindow(), backend_api, enable_validation});
        const auto camera_type = RenderSettings::GetRenderingPipelineType() == RenderingPipelineType::Only2D
            ? CameraType::Orthographic : CameraType::Perspective;
        m_camera = Camera::Create({camera_type, m_viewport_manager->getLogicalSize(), m_viewport_manager->getWindowSize()});
        m_descriptor_table = DescriptorTableManager::Create({m_gfx.get()});
        m_texture_manager = TextureManager::Create({m_gfx.get(), m_descriptor_table.get()});

        if (!Renderer::Initialize(m_camera.get(), m_texture_manager.get())) {
            return false;
        }
        m_rendering_pipeline = RenderingPipeline::Create({
            std::thread::hardware_concurrency(),
            m_gfx.get(),
            m_descriptor_table.get(),
            m_texture_manager.get()
        });

        return m_camera && m_descriptor_table && m_texture_manager && m_rendering_pipeline;
    }

    void RenderSystem::shutdown() {
        if (m_gfx && m_gfx->getDevice()) {
            m_gfx->getDevice()->waitForIdle();
        }

        RenderingPipeline::Destroy(m_rendering_pipeline);
        Renderer::Shutdown();
        Camera::Destroy(m_camera);
        TextureManager::Destroy(m_texture_manager);
        DescriptorTableManager::Destroy(m_descriptor_table);
        if (m_gfx && m_gfx->getDevice()) {
            m_gfx->getDevice()->waitForIdle();
            m_gfx->getDevice()->runGarbageCollection();
        }
        GfxContext::Destroy(m_gfx);
    }

} // dodoe
