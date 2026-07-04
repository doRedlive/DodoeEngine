// do@Redlive

#include "sandbox_layer.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"

#include "runtime/function/script/script_system.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/service/scene_importer.h"
#include "runtime/core/utils/common.h"


namespace sandbox {

    SandboxLayer::SandboxLayer(const std::string& name)
        : dodoe::Layer(name) {
    }

    void SandboxLayer::attach() {
        auto* world = dodoe::GetWorld();
        auto scene = world->getCurrentScene();

        auto test_go = scene->createEntity("test_go");
        auto& transform = test_go.getComponent<dodoe::TransformComponent>();
        transform.position = {0.0f, 0.0f, 0.0f};
        transform.scale = {1.0f, 1.0f, 1.0f};
        auto& sprite_renderer = test_go.addComponent<dodoe::SpriteRendererComponent>();
        const dodoe::String texture_path = "engine/res/pictures/grm.jpg";
        sprite_renderer.texture = dodoe::PPtr<dodoe::Texture>(dodoe::FileID(texture_path), dodoe::UUID(dodoe::string2hash(texture_path)));

        dodoe::SceneImporter::ImportModel("engine/res/models/backpack/backpack.obj");
    }
    
    void SandboxLayer::detach() {
        
    }
    
    void SandboxLayer::updateTick(const float delta_time) {

    }

    void SandboxLayer::renderTick() {
    }

} // namespace sandbox