// do@Redlive

#include "sandbox_layer.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"

#include "runtime/function/script/script_system.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/service/world/scene_importer.h"
#include "runtime/function/render/texture/sprite_manager.h"
#include "runtime/core/utils/common.h"


namespace sandbox {

    SandboxLayer::SandboxLayer(const std::string& name)
        : dodoe::Layer(name.c_str()) {
    }

    void SandboxLayer::attach() {
        auto world = dodoe::GetWorld();
        auto cur_scene = world->getActiveScene();

        // auto tex = dodoe::Texture2D::Load("engine/res/pictures/grm.jpg");
        // if (!tex) {
        //     DO_ERROR("Failed to load texture!");
        //     return;
        // }

        // {
        //     auto entity = cur_scene->createEntity("Sandbox");
        //     auto& transform = entity.getComponent<dodoe::TransformComponent>();
        //     transform.setPosition({0.0f, 0.0f, 0.0f});
        //     transform.setScale({5.0f, 5.0f, 1.0f});

        //     auto& sr = entity.addComponent<dodoe::SpriteRendererComponent>();
        //     sr.pivot = {0.5f, 0.5f};
        //     sr.depth = 0.0f;
        //     sr.sprite = dodoe::PPtr<dodoe::Sprite>(dodoe::SpriteLoader::Load(tex->getPath()));
        //     sr.dirty = true;
        // }

        // {
        //     auto entity = cur_scene->createEntity("test_go");
        //     auto& transform = entity.getComponent<dodoe::TransformComponent>();
        //     transform.setPosition({1.0f, 0.0f, 0.0f});
        //     transform.setScale({5.0f, 5.0f, 1.0f});

        //     // auto& sr = entity.addComponent<dodoe::SpriteRendererComponent>();
        //     // sr.pivot = {0.5f, 0.5f};
        //     // sr.depth = 0.0f;
        //     // sr.sprite = dodoe::PPtr<dodoe::Sprite>(dodoe::SpriteLoader::Load(tex->getPath()));
        //     // sr.dirty = true;
        // }

        // dodoe::SceneImporter::ImportModel("engine/res/models/backpack/backpack.obj");
    }
    
    void SandboxLayer::detach() {
        
    }
    
    void SandboxLayer::updateTick(const float delta_time) {

    }

    void SandboxLayer::renderTick() {
    }

} // namespace sandbox