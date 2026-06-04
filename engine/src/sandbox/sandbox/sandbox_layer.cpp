//
// Sandbox runtime layer.
//

#include "sandbox_layer.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"

#include "runtime/function/script/script_system.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/function/render/renderer_2d.h"

namespace sandbox {

    SandboxLayer::SandboxLayer(const std::string& name)
        : dodoe::Layer(name) {
    }

    void SandboxLayer::attach() {
        dodoe::Application::Self().context().script_system->executeLua("engine/res/scripts/test.lua");
        // dodoe::Application::self().context().script_system->execute_csharp("engine/src/scriptcore/bin/Debug/net8.0/GreenCake.dll");
        auto& world = dodoe::Application::Self().context().world;
        auto scene = world->getCurrentScene();

        auto test_go = scene->createEntity("test_go");
        auto& transform = test_go.getComponent<dodoe::TransformComponent>();
        transform.position = {0.0f, 0.0f, 0.0f};
        transform.scale = {1.0f, 1.0f, 1.0f};
        auto& sprite_renderer = test_go.addComponent<dodoe::SpriteRendererComponent>();
        const String texture_path = "engine/res/pictures/grm.jpg";
        sprite_renderer.texture = PPtr<Texture>(FileID(texture_path), UUID(static_cast<UInt64>(string2hash(texture_path))));
        // sprite_renderer.pivot = dodoe::Vector2f(0.5f, 0.5f);

    }
    
    void SandboxLayer::detach() {
        
    }
    
    void SandboxLayer::updateTick(const float delta_time) {
        dodoe::Renderer2D::DrawLine({0.0f, 0.0f}, {300.0f, 0.0f}, {0.0f, 0.0f, 100.0f}, 1.5f, dodoe::Color::green());
        dodoe::Renderer2D::DrawRect({5.0f, 0.0f}, {100.0f, 100.0f}, {0.0f, 0.0f, 0.0f}, dodoe::Color::blue(), 2.0f);
        dodoe::Renderer2D::DrawRect({-5.0f, 0.0f}, {100.0f, 100.0f}, {0.0f, 0.0f, 0.0f}, dodoe::Color::blue(), 2.0f);


        dodoe::Renderer2D::DrawLine({-5.0f, 5.0f}, {5.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, 1.5f, dodoe::Color::green());
        // dodoe::Renderer::drawLine({-5.0f, 5.0f}, {-5.0f, -5.0f}, {0.0f, 0.0f, 0.0f}, 1.5f, dodoe::Color::green());
        // dodoe::Renderer::drawLine({-5.0f, -5.0f}, {5.0f, -5.0f}, {0.0f, 0.0f, 0.0f}, 1.5f, dodoe::Color::green());
        // dodoe::Renderer::drawLine({5.0f, -5.0f}, {5.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, 1.5f, dodoe::Color::green());
    }

    void SandboxLayer::renderTick() {
    }

} // namespace sandbox
