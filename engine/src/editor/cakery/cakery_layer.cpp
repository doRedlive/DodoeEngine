//
// Created by GreenMuffin on 2025/12/7.
//

#include "cakery_layer.h"
#include "simulation_frame_context_system.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/function/input/input.h"
#include "runtime/function/render/framework/camera.h"
#include "runtime/function/render/framework/scene_graph.h"
#include "runtime/function/render/render_resource.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/renderer_2d.h"
#include "runtime/resource/resource_manager.h"

#include "runtime/function/render/framework/scene_importer.h"

#include "imgui/imgui.h"

#include <iomanip>
#include <sstream>

using namespace dodoe;

namespace cakery {
    CakeryLayer::CakeryLayer(const std::string& name) : Layer(name) { }

    void CakeryLayer::enterEditor() {
        auto& context = Application::Self().context();
        context.startRuntime();
        const auto& world = context.world;
        world->registerSimulationSystem(create_ref<SimulationFrameContextSystem>());
        m_hierarchy_panel.setContext(world->getCurrentScene());
        m_viewport_panel.initialize();
        m_editor_initialized = true;
    }

    void CakeryLayer::attach() {
        m_window = Application::Self().context().window_manager->getWindow();
        m_base_title = Application::Self().specification().name;
        m_editor_initialized = false;
    }

    void CakeryLayer::detach() {
        if (m_window && m_window->getNativeWindow()) {
            glfwSetWindowTitle(m_window->getNativeWindow(), m_base_title.c_str());
        }
        if (m_editor_initialized) {
            m_project_panel.cleanup();
            m_viewport_panel.cleanup();
            Application::Self().context().finalizeRuntime();
        }
    }

    void CakeryLayer::updateTick(const float dt) {
        if (m_window && m_window->getNativeWindow()) {
            char title[64]{};
            std::snprintf(title, sizeof(title), "%s - %.1f FPS", m_base_title.c_str(), dt > 0.0f ? 1.0f / dt : 0.0f);
            glfwSetWindowTitle(m_window->getNativeWindow(), title);
        }

        if (m_editor_initialized) {
            m_viewport_panel.update();
        }
    }

    void CakeryLayer::renderTick() {
        if (!m_editor_initialized) {
            if (m_project_manager_panel.draw()) {
                enterEditor();
            }
            return;
        }

        m_title_bar.draw(m_window);
        m_dockspace_panel.draw();
        m_hierarchy_panel.draw();
        m_inspector_panel.draw();
        m_project_panel.draw();
        m_console_panel.draw();
        m_viewport_panel.draw();
    }
} // cakery
