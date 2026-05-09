#pragma once

#include "dopch.h"

#include "helper/editor_camera_controller.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_resource.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/world/systems/system.h"

namespace cakery {

    class SimulationFrameContextSystem final : public dodoe::System {
        dodoe::Scope<EditorCameraController> editor_camera_controller_{nullptr};
    public:
        ~SimulationFrameContextSystem() override {
            EditorCameraController::Destroy(editor_camera_controller_);
        }

        void update(dodoe::Registry& reg, float dt) override {
            (void)reg;
            ensureController();
            if (!editor_camera_controller_) {
                return;
            }
            auto* editor_camera = editor_camera_controller_->camera();
            if (!editor_camera) {
                return;
            }

            auto* render_system = dodoe::Application::Self().context().render_system.get();
            if (render_system) {
                auto* viewport_manager = render_system->getViewportManager();
                if (viewport_manager) {
                    editor_camera->setViewportSize(
                        viewport_manager->getLogicalSize(),
                        viewport_manager->getWindowSize()
                    );
                }
            }

            editor_camera_controller_->onUpdate(dt);

            dodoe::g_RenderResource->submitMainCameraViewProjection(editor_camera->getViewProjectionMatrix(), editor_camera->getPosition());
        }

    private:
        void ensureController() {
            if (!editor_camera_controller_) {
                editor_camera_controller_ = EditorCameraController::Create({});
            }
        }
    };

} // cakery
