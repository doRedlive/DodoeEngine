//
// Sandbox runtime layer.
//

#include "sandbox_layer.h"

#include "runtime/core/world/world_manager.h"
#include "runtime/core/world/entity.h"
#include "runtime/core/world/components.h"

namespace sandbox {

    SandboxLayer::SandboxLayer(const std::string& name)
        : dodoe::Layer(name) {
    }

    void SandboxLayer::on_attach() {
        auto& world = dodoe::WorldManager::self().active_world();

        auto scene = world.active_scene();

        auto test_go = scene->create_entity("test_go");
        auto& transform = test_go.get_component<dodoe::TransformComponent>();
        transform.position = {0.0f, 0.0f, 0.0f};
        transform.scale = {1.0f, 1.0f, 1.0f};
        auto& sprite_renderer = test_go.add_component<dodoe::SpriteRendererComponent>();
        sprite_renderer.texture_path = "engine/res/pictures/grm.jpg";
        sprite_renderer.pivot = dodoe::Vector2f(0.5f, 0.5f);
    }

    void SandboxLayer::on_detach() {

    }

    void SandboxLayer::on_update(const float delta_time) {

    }

    void SandboxLayer::on_ui_render() {
    }

} // namespace sandbox
