// do@Redlive
// Knight!

#include "render_system.h"

#include "render_settings.h"
#include "runtime/function/graphics/draw_command_list.h"

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
        m_gfx = GfxContext::Create({window->getNativeWindow(), backend_api, enable_validation, window->isHostMode() ? window->getNativeHandle() : nullptr});
        GDrawCommandList.setDevice(m_gfx->getDevice());
        m_descriptor_table = DescriptorTableManager::Create({m_gfx.get()});
        m_texture_manager = TextureManager::Create({m_gfx.get(), m_descriptor_table.get()});
        m_shared_render_service = SharedRenderService::Create({m_gfx.get(), m_descriptor_table.get(), m_texture_manager.get()});
        m_render_scene = RenderScene::Create({});
        m_view_family = RenderViewFamily::Create({});
        m_render_pipeline = RenderPipeline::Create({
            std::thread::hardware_concurrency(),
            m_gfx.get(),
            m_shared_render_service.get()
        });

        return m_render_scene && m_descriptor_table && m_texture_manager && m_shared_render_service && m_render_pipeline;
    }

    void RenderSystem::shutdown() {
        m_gfx->waitForIdle();

        RenderPipeline::Destroy(m_render_pipeline);
        RenderViewFamily::Destroy(m_view_family);
        RenderScene::Destroy(m_render_scene);
        SharedRenderService::Destroy(m_shared_render_service);
        TextureManager::Destroy(m_texture_manager);
        DescriptorTableManager::Destroy(m_descriptor_table);
        m_gfx->waitForIdle();
        m_gfx->clearGarbage();
        GfxContext::Destroy(m_gfx);
    }

} // dodoe
