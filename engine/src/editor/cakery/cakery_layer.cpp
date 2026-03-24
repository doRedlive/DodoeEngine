//
// Created by GreenMuffin on 2025/12/7.
//

#include "cakery_layer.h"

#include "panels/title_bar.h"

#include "runtime/core/application.h"
#include "runtime/core/world/components.h"
#include "runtime/core/world/world_manager.h"

#include "runtime/function/context.h"
#include "runtime/function/input/input.h"
#include "runtime/function/render/camera/camera.h"

#include "imgui/imgui.h"

using namespace dodoe;

namespace cakery {
    CakeryLayer::CakeryLayer(const std::string& name): Layer(name) {
        cakery_window_ = g_context.window_manager->active_window();     //TODO: FIXME;
    }

    CakeryLayer::~CakeryLayer() { }

    void CakeryLayer::on_attach() {
        auto& world_manager = WorldManager::self();
        if (world_manager.world_count() == 0) {
            world_manager.create_world("default");
        }
        auto& default_world = world_manager.active_world();

        const auto scene = default_world.active_scene();
        hierarchy_panel_.set_context(scene);

        const auto test_go = scene->create_game_object("test_go");
        auto& transform = test_go->get_component<TransformComponent>();
        transform.position = {200.0f, 200.0f, 0.0f};
        transform.scale = {200.0f, 200.0f, 1.0f};
        auto& sprite_renderer = test_go->add_component<SpriteRendererComponent>();

        Camera::instance().set_background_color(Color::gray());
    }

    void CakeryLayer::on_detach() {

    }

    void CakeryLayer::on_update(float delta_time) {
        if (dodoe::Input::is_key_pressed(dodoe::KeyCode::A)) {
            LogDebug("the a is pressed");
        }
        //viewport_panel_.on_update();
    }

    void CakeryLayer::on_ui_render() {
        hierarchy_panel_.on_ui_render();
        inspector_panel_.on_ui_render();
        project_panel_.on_ui_render();
        console_panel_.on_ui_render();

        if (Application::self().specification().custom_titlebar) {
            Titlebar titlebar;
            titlebar.draw(cakery_window_);
        }

        // tests
        ImGui::Begin("FPS");
        auto fps = dodoe::g_context.time_system->get_fps();
        ImGui::Text("fps: %d", fps);
        ImGui::End();
    }
}
