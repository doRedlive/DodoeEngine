//
// Created by GreenMuffin on 2026/3/6.
// Knight!
//

#include "render_system.h"

#include "render_api.h"
#include "render_resource.h"

#include "framework/texture_manager.h"

#include "runtime/core/utils/util.h"

namespace dodoe {

    Scope<RenderSystem> RenderSystem::create(const RenderSystemCreateInfo& info) { 
        if (auto context = create_scope<RenderSystem>(); context->initialize(info)) 
            return context;
        return nullptr;
    }

    void RenderSystem::destroy(Scope<RenderSystem>& system) {
        if (!system) return;
        system->shutdown();
        system.reset();
    }

    bool RenderSystem::initialize(const RenderSystemCreateInfo& init_info) {
        m_window_manager = init_info.window_manager;
        m_ui_system = init_info.ui_system;

        auto window = m_window_manager->window();

        RenderApi::initialize({init_info.backend_api});
        m_viewport_manager = ViewportManager::create({window});
        m_rhi = RhiContext::create({window->nativeWindow(), init_info.backend_api, true});
        m_camera = Camera::create({CameraType::Orthographic, m_viewport_manager->getLogicalSize(), m_viewport_manager->getWindowSize()});
        m_descriptor_table = DescriptorTableManager::create({m_rhi.get()});
        m_texture_manager = TextureManager::create({m_rhi.get(), m_descriptor_table.get()});

        g_RenderResource->initilize(m_rhi->getDevice());

        m_render_graph = RenderGraph::create({m_rhi.get(), m_camera.get(), m_ui_system, m_descriptor_table.get()});

        m_render_graph->setup();
        m_render_graph->compile();

        return m_camera && m_descriptor_table && m_texture_manager && m_render_graph;
    }

    void RenderSystem::shutdown() {
        RenderGraph::destroy(m_render_graph);
        Camera::destroy(m_camera);
        g_RenderResource->shutdown();
        TextureManager::destroy(m_texture_manager);
        DescriptorTableManager::destroy(m_descriptor_table);
		if (m_rhi && m_rhi->getDevice()) {
			m_rhi->getDevice()->waitForIdle();
			m_rhi->getDevice()->runGarbageCollection();
		}
        RhiContext::destroy(m_rhi);
    }

    void RenderSystem::prepare() {
        m_viewport_manager->update();
        if (m_viewport_manager->isViewportDirty()) [[unlikely]] {
            m_camera->setViewportSize(m_viewport_manager->getLogicalSize(), m_viewport_manager->getWindowSize());
            m_render_graph->onViewportResize(m_viewport_manager->viewport());
        }
        if (m_viewport_manager->isWindowDirty()) [[unlikely]] {
		    m_rhi->recreateSwapchain();
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

        m_rhi->getDevice()->waitForIdle();
        if (!m_rhi->presentSwapchainImage(image_index)) {
            return;
        }
        m_rhi->getDevice()->runGarbageCollection();
    }

    void RenderSystem::swapLogicRenderContext() {
        g_RenderResource->swapLogicRenderContext();
    }

} // dodoe

