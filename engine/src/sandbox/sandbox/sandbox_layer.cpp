//
// Sandbox runtime layer.
//

#include "sandbox_layer.h"

#include "core/application.h"
#include "core/context/system_context.h"

#include "runtime/function/world/world.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/function/render/renderer_2d.h"

namespace sandbox {

    SandboxLayer::SandboxLayer(const std::string& name)
        : dodoe::Layer(name) {
    }

    void SandboxLayer::on_attach() {
        auto& world = dodoe::Application::self().context().world;
        auto scene = world->active_scene();

        auto test_go = scene->create_entity("test_go");
        auto& transform = test_go.get_component<dodoe::TransformComponent>();
        transform.position = {0.0f, 0.0f, 0.0f};
        transform.scale = {1.0f, 1.0f, 1.0f};
        // auto& sprite_renderer = test_go.add_component<dodoe::SpriteRendererComponent>();
        // const std::string texture_path = "engine/res/pictures/grm.jpg";
        // const auto texture_res = dodoe::ResourceManager::self().get_texture(texture_path, texture_path);
        // sprite_renderer.texture_id = texture_res.texture_id;
        // sprite_renderer.pivot = dodoe::Vector2f(0.5f, 0.5f);

    }
    
    void SandboxLayer::on_detach() {
        
    }
    
    void SandboxLayer::on_update(const float delta_time) {
        dodoe::Renderer2d::drawLine({0.0f, 0.0f}, {300.0f, 0.0f}, {0.0f, 0.0f, 100.0f}, 1.5f, dodoe::Color::green());
        // dodoe::Renderer::drawRect({5.0f, 0.0f}, {100.0f, 100.0f}, {0.0f, 0.0f, 0.0f}, dodoe::Color::blue(), 2.0f);
        // dodoe::Renderer::drawRect({-5.0f, 0.0f}, {100.0f, 100.0f}, {0.0f, 0.0f, 0.0f}, dodoe::Color::blue(), 2.0f);


        dodoe::Renderer2d::drawLine({-5.0f, 5.0f}, {5.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, 1.5f, dodoe::Color::green());
        // dodoe::Renderer::drawLine({-5.0f, 5.0f}, {-5.0f, -5.0f}, {0.0f, 0.0f, 0.0f}, 1.5f, dodoe::Color::green());
        // dodoe::Renderer::drawLine({-5.0f, -5.0f}, {5.0f, -5.0f}, {0.0f, 0.0f, 0.0f}, 1.5f, dodoe::Color::green());
        // dodoe::Renderer::drawLine({5.0f, -5.0f}, {5.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, 1.5f, dodoe::Color::green());
    }

    void SandboxLayer::on_render() {
    }

} // namespace sandbox
