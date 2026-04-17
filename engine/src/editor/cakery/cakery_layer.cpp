//
// Created by GreenMuffin on 2025/12/7.
//

#include "cakery_layer.h"

#include "panels/title_bar.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/function/input/input.h"
#include "runtime/function/render/framework/camera.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/renderer_2d.h"
#include "runtime/resource/resource_manager.h"

#include "imgui/imgui.h"

using namespace dodoe;

namespace cakery {
    CakeryLayer::CakeryLayer(const std::string& name)
        : Layer(name) {
    }

    CakeryLayer::~CakeryLayer() { }

    void CakeryLayer::attach() {
        const auto& world = Application::self().context().world;
        auto* scene = world->active_scene();
        hierarchy_panel_.setContext(scene);

        bool has_marry_entity = false;
        for (auto entity : scene->getEntities()) {
            if (!entity.hasComponent<dodoe::TagComponent>()) {
                continue;
            }
            if (entity.getComponent<dodoe::TagComponent>().getTag() == "MarryModel") {
                has_marry_entity = true;
                break;
            }
        }

        if (!has_marry_entity) {
            dodoe::Entity model_entity = scene->create_entity("MarryModel");
            model_entity.getComponent<dodoe::TagComponent>().setTag("MarryModel");

            auto& transform = model_entity.getComponent<dodoe::TransformComponent>();
            transform.position = {0.0f, 0.0f, 0.0f};
            transform.rotation = {0.0f, 0.0f, 0.0f};
            transform.scale = {1.0f, 1.0f, 1.0f};

            const std::filesystem::path model_path = std::filesystem::path(DODOE_ROOT) / "engine" / "res" / "models" / "marry" / "Marry.obj";
            auto model_res = dodoe::ResourceManager::self().get_model("cakery_marry", model_path.string());
            if (model_res.data) {
                auto& model_renderer = model_entity.addComponent<dodoe::ModelRendererComponent>();
                model_renderer.model_id = model_res.id;
                model_renderer.color = dodoe::Color::white();
            }
        }
		viewport_panel_.initialize();
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

        viewport_panel_.update();
    }

    void CakeryLayer::renderTick() {
        dockspace_panel_.draw();
        hierarchy_panel_.draw();
        inspector_panel_.draw();
        project_panel_.draw();
        console_panel_.draw();
        viewport_panel_.draw();

        if (Application::self().specification().custom_titlebar) {
            // Titlebar titlebar;
            // titlebar.draw(cakery_window_);
        }
    }
}
