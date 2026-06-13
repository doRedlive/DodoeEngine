#include "camera2d_system.h"

#include "render_system_bridge.h"

namespace dodoe {

    Camera2dSystem::~Camera2dSystem() = default;

    void Camera2dSystem::update(Registry& reg, float dt) {
        (void)dt;

        auto* camera = TryGetMainCamera();
        if (!camera) {
            return;
        }

        auto view = reg.view<Camera2dComponent, TransformComponent, TagComponent>();
        for (auto entity : view) {
            const auto tag_id = reg.get<TagComponent>(entity).id;
            if (tag_id != tag::PrimaryCameraTag) {
                continue;
            }

            auto& camera_comp = reg.get<Camera2dComponent>(entity);
            auto& transform_comp = reg.get<TransformComponent>(entity);
            if (!camera_comp.dirty && !transform_comp.dirty) {
                continue;
            }

            camera->setCameraType(camera_comp.getCameraType());
            camera->setPosition(transform_comp.position);
            camera->setRotation(transform_comp.rotation.z);
            camera->setZoom(camera_comp.zoom);
            camera->setClearColor(camera_comp.background);
            SubmitMainCameraViewProjection(camera->getViewProjectionMatrix(), camera->getPosition());
            camera_comp.dirty = false;
            transform_comp.dirty = false;
        }
    }

} // dodoe
