//
// Created by GreenMuffin on 2025/12/7.
//

#include "cakery_layer.h"

#include "panels/title_bar.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/input/input.h"
#include "runtime/function/render/camera/camera.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/renderer_2d.h"

#include "imgui/imgui.h"

using namespace dodoe;

namespace cakery {
    CakeryLayer::CakeryLayer(const std::string& name)
        : Layer(name)
        , viewport_panel_(
            Application::self().context().render_system->rhiBackend(),
            Application::self().context().render_system->mainSceneTextures()) {
    }

    CakeryLayer::~CakeryLayer() { }

    void CakeryLayer::attach() {

    }

    void CakeryLayer::detach() {
        project_panel_.cleanup();
        viewport_panel_.cleanup();
    }

    void CakeryLayer::updateTick(float delta_time) {
		(void)delta_time;
		dodoe::Renderer2d::drawLine({-300.0f, 0.0f}, {300.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 2.0f, dodoe::Color::green());
		dodoe::Renderer2d::drawLine({0.0f, -200.0f}, {0.0f, 200.0f}, {0.0f, 0.0f, 0.0f}, 2.0f, dodoe::Color::blue());
		dodoe::Renderer2d::drawRect({-80.0f, -80.0f}, {160.0f, 160.0f}, {0.0f, 0.0f, 0.0f}, dodoe::Color::white(), 3.0f);

        auto* render_system = Application::self().context().render_system.get();
        viewport_panel_.setTextures(render_system->mainSceneTextures());
        viewport_panel_.setCurrentFramebufferIndex(render_system->currentSwapchainImageIndex());
        viewport_panel_.update();
    }

    void CakeryLayer::renderTick() {
        dockspace_panel_.draw();
        hierarchy_panel_.draw();
        // inspector_panel_.draw();
        project_panel_.draw();
        console_panel_.draw();
        viewport_panel_.draw();

        if (Application::self().specification().custom_titlebar) {
            // Titlebar titlebar;
            // titlebar.draw(cakery_window_);
        }
    }
}
