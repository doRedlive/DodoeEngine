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
#include "runtime/function/render/pixel2d/sprite_manager.h"
#include "runtime/function/input/input_types.h"
#include "runtime/function/input/key_code.h"
#include "runtime/function/input/mouse_code.h"
#include "runtime/core/utils/common.h"


namespace sandbox {

    SandboxLayer::SandboxLayer(const std::string& name)
        : dodoe::Layer(name.c_str()),
          m_camera(std::make_unique<SandboxCamera>()),
          m_provider(m_camera.get()) {
    }

    void SandboxLayer::attach() {
        auto* input = dodoe::GetInputManager();
        if (!input) {
            return;
        }

        input->registerActionMap("Sandbox");
        input->pushInputContext("Sandbox");

        input->registerAction("Sandbox", "Move", dodoe::InputActionValueType::Axis2D);
        input->bindKey2D("Sandbox", "Move", dodoe::KeyCode::W, {0.0f, 1.0f});
        input->bindKey2D("Sandbox", "Move", dodoe::KeyCode::S, {0.0f, -1.0f});
        input->bindKey2D("Sandbox", "Move", dodoe::KeyCode::A, {-1.0f, 0.0f});
        input->bindKey2D("Sandbox", "Move", dodoe::KeyCode::D, {1.0f, 0.0f});

        input->registerAction("Sandbox", "Look", dodoe::InputActionValueType::Button);
        input->bindMouseButton("Sandbox", "Look", dodoe::MouseCode::ButtonRight);

        input->registerAction("Sandbox", "Up", dodoe::InputActionValueType::Button);
        input->bindKey("Sandbox", "Up", dodoe::KeyCode::E);

        input->registerAction("Sandbox", "Down", dodoe::InputActionValueType::Button);
        input->bindKey("Sandbox", "Down", dodoe::KeyCode::Q);

        if (auto* render_system = dodoe::GetRenderSystem()) {
            if (auto* view_manager = render_system->getViewManager()) {
                for (auto& target : view_manager->getTargets()) {
                    target->setCamera(&m_provider);
                }
            }
        }

        // auto* world = dodoe::GetWorld();
        // if (!world) {
        //     return;
        // }
        // auto* cur_scene = world->getActiveScene();
        // (void)cur_scene;

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
        if (auto* input = dodoe::GetInputManager()) {
            input->popInputContext("Sandbox");
        }
        if (auto* render_system = dodoe::GetRenderSystem()) {
            if (auto* view_manager = render_system->getViewManager()) {
                for (auto& target : view_manager->getTargets()) {
                    if (target->getCamera() == &m_provider) {
                        target->setCamera(nullptr);
                    }
                }
            }
        }
    }

    void SandboxLayer::updateTick(const float delta_time) {
        if (auto* window_manager = dodoe::GetWindowManager()) {
            if (auto* window = window_manager->getWindow()) {
                m_camera->setViewportSize(
                    static_cast<float>(window->getWidth()),
                    static_cast<float>(window->getHeight()));
            }
        }
        m_camera->update(delta_time);
    }

    void SandboxLayer::renderTick() {
    }

} // namespace sandbox