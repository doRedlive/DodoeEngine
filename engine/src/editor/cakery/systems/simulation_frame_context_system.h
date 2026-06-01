// do@Redlive

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
        dodoe::Scope<EditorCameraController> m_editor_camera_controller{nullptr};
    public:
        ~SimulationFrameContextSystem() override {
            EditorCameraController::Destroy(m_editor_camera_controller);
        }

        void update(dodoe::Registry& reg, float dt) override {
            (void)reg;
            if (!m_editor_camera_controller) {
                auto& main_camera = dodoe::Application::Self().context().getRenderSystem()->getMainCamera();
                m_editor_camera_controller = EditorCameraController::Create({&main_camera});
            }

            auto* editor_camera = m_editor_camera_controller->getCamera();
            if (!editor_camera) return;

            auto render_system = dodoe::Application::Self().context().getRenderSystem();
            if (render_system) {
                auto viewport_manager = render_system->getViewportManager();
                if (viewport_manager) {
                    editor_camera->setViewportSize(
                        viewport_manager->getLogicalSize(),
                        viewport_manager->getWindowSize()
                    );
                }
            }

            m_editor_camera_controller->onUpdate(dt);

            dodoe::g_RenderResource->submitMainCameraViewProjection(editor_camera->getViewProjectionMatrix(), editor_camera->getPosition());
        }
    };

} // cakery
