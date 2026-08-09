// do@Redlive

#include "camera_system.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/math/math.h"
#include "runtime/core/channel/camera_channel.h"
#include "runtime/core/utils/tags.h"

namespace dodoe {

    CameraSystem::~CameraSystem() = default;

    SystemAccess CameraSystem::getAccess() const {
        return SystemAccessBuilder{}
            .readsComponents<CameraComponent, TransformComponent, TagComponent>()
            .build();
    }

    void CameraSystem::update(Registry& reg, float dt) {
        (void)dt;

        auto view = reg.view<CameraComponent, TransformComponent, TagComponent>();

        auto& registry = GetCameraRegistry();
        Size_t camera_index = 0;

        float screen_aspect = 16.0f / 9.0f;
        if (auto* window_manager = GetWindowManager(); window_manager) {
            if (auto* window = window_manager->getWindow(); window) {
                const Vector2i pixel = window->getPixelSize();
                if (pixel.x > 0 && pixel.y > 0) {
                    screen_aspect = static_cast<float>(pixel.x) / static_cast<float>(pixel.y);
                }
            }
        }
        for (auto entity : view) {
            if (camera_index >= kMaxCameras) {
                break;
            }

            auto& cam = reg.get<CameraComponent>(entity);
            auto& tf  = reg.get<TransformComponent>(entity);

            const auto pos = tf.position;
            const auto rot = tf.rotation;

            if (cam.type == CameraType::Perspective) {
                const Vector3f eye(pos.x, pos.y, pos.z);
                const Vector3f center(pos.x + Math::Sin(Math::Radians(rot.y)) * Math::Cos(Math::Radians(rot.x)),
                                      pos.y + Math::Sin(Math::Radians(rot.x)),
                                      pos.z - Math::Cos(Math::Radians(rot.y)) * Math::Cos(Math::Radians(rot.x)));
                const Vector3f up(0.0f, 1.0f, 0.0f);
                cam.view_matrix = Math::LookAt(eye, center, up);
                cam.projection_matrix = Math::Perspective(Math::Radians(cam.fov), cam.aspect_ratio, cam.near_plane, cam.far_plane);
            } else {
                const Vector3f eye(pos.x, pos.y, 10.0f);
                const Vector3f center(pos.x, pos.y, 0.0f);
                cam.view_matrix = Math::LookAt(eye, center, Vector3f(0.0f, 1.0f, 0.0f));
                const float half_h = (360.0f * 0.5f) / cam.zoom;
                const float half_w = half_h * screen_aspect;
                cam.projection_matrix = Math::Ortho(-half_w, half_w, -half_h, half_h, cam.near_plane, cam.far_plane);
            }

            registry.cameras[camera_index].view = cam.view_matrix;
            registry.cameras[camera_index].projection = cam.projection_matrix;
            ++camera_index;

            cam.dirty = false;
            tf.dirty = false;
        }

        registry.active_count = static_cast<UInt32>(camera_index);
    }

} // dodoe
