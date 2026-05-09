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
    CakeryLayer::CakeryLayer(const std::string& name)
        : Layer(name) {
    }

    CakeryLayer::~CakeryLayer() { }

    void CakeryLayer::attach() {
        cakery_window_ = Application::Self().context().window_manager->window();
        base_window_title_ = Application::Self().specification().name;
        fps_accumulated_time_ = 0.0f;
        fps_frame_counter_ = 0;

        const auto& world = Application::Self().context().world;
        if (!simulation_frame_context_registered_) {
            world->registerSimulationSystem(create_ref<SimulationFrameContextSystem>());
            simulation_frame_context_registered_ = true;
        }
        auto* scene = world->getCurrentScene();
        hierarchy_panel_.setContext(scene);

        // dodoe::SceneImporter::ImportModel("engine/res/models/marry/marry.obj");
        std::unordered_set<dodoe::Uuid> existing_entities;
        for (auto entity : scene->getEntities()) {
            existing_entities.insert(entity.uuid());
        }

        dodoe::SceneImporter::ImportModel("engine/res/models/backpack/backpack.obj");

        dodoe::Entity backpack_root;
        for (auto entity : scene->getEntities()) {
            if (existing_entities.contains(entity.uuid())) {
                continue;
            }

            if (entity.name() == "backpack") {
                backpack_root = entity;
                break;
            }
        }

        dodoe::SceneImporter::ImportModel("engine/res/models/quad.obj");

        if (!scene_light_created_) {
            auto light_entity = scene->createEntity("Scene Light");
            auto& transform = light_entity.getComponent<dodoe::TransformComponent>();
            transform.position = {0.0f, 5.0f, 0.0f};

            auto& light = light_entity.addComponent<dodoe::PointLightComponent>();
            light.intensity = 5.0f;
            light.range = 20.0f;
            light.radius = 0.25f;

            auto& render_scene = dodoe::g_RenderResource->getRenderScene();
            const auto scene_graph = render_scene.getSceneGraph();
            if (scene_graph) {
                auto directional_light_node = scene_graph->createNode(scene_graph->getRoot());
                directional_light_node->setName("EditorDirectionalLight");
                directional_light_node->setRotation({-55.0f, 35.0f, 0.0f});

                auto directional_light = dodoe::create_ref<dodoe::DirectionalLight>();
                directional_light->color = dodoe::Color::white();
                directional_light->irradiance = 1.0f;
                directional_light_node->setLeaf(directional_light);
            }

            scene_light_created_ = true;
        }

        for (auto entity : scene->getEntities()) {
            if (entity.name() == "quad") {
                auto& transform = entity.getComponent<dodoe::TransformComponent>();
                transform.position = {0.0f, -2.0f, 0.0f};
                transform.scale = {8.0f, 1.0f, 8.0f};
                break;
            }
        }

		viewport_panel_.initialize();
    }

    void CakeryLayer::detach() {
        if (cakery_window_ && cakery_window_->nativeWindow()) {
            glfwSetWindowTitle(cakery_window_->nativeWindow(), base_window_title_.c_str());
        }
        project_panel_.cleanup();
        viewport_panel_.cleanup();
    }

    void CakeryLayer::updateTick(float delta_time) {
        fps_accumulated_time_ += delta_time;
        ++fps_frame_counter_;
        if (fps_accumulated_time_ >= 1.0f && cakery_window_ && cakery_window_->nativeWindow()) {
            const float fps = static_cast<float>(fps_frame_counter_) / fps_accumulated_time_;
            std::ostringstream title_stream;
            title_stream << base_window_title_ << " | FPS: " << std::fixed << std::setprecision(1) << fps;
            glfwSetWindowTitle(cakery_window_->nativeWindow(), title_stream.str().c_str());
            fps_accumulated_time_ = 0.0f;
            fps_frame_counter_ = 0;
        }
        viewport_panel_.update();
    }

    void CakeryLayer::renderTick() {
        title_bar_.draw(cakery_window_);
        dockspace_panel_.draw();
        hierarchy_panel_.draw();
        inspector_panel_.draw();
        project_panel_.draw();
        console_panel_.draw();
        viewport_panel_.draw();
    }
} // cakery
