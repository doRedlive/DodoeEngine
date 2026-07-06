// do@Redlive

#include "camera_system.h"

#include "runtime/core/math/math.h"
#include "runtime/core/utils/tags.h"
#include "runtime/core/channel/render_channel.h"

namespace dodoe {

    CameraSystem::~CameraSystem() = default;

    void CameraSystem::update(Registry& reg, float dt) {
        (void)dt;

        auto view = reg.view<Camera2dComponent, TransformComponent, TagComponent>();

        for (auto entity : view) {
            const auto tag_id = reg.get<TagComponent>(entity).id;
            if (tag_id != tag::PrimaryCameraTag) continue;

            auto& cam = reg.get<Camera2dComponent>(entity);
            auto& tf  = reg.get<TransformComponent>(entity);
            if (!cam.dirty && !tf.dirty) continue;

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
                const float half_w = (640.0f * 0.5f) / cam.zoom;
                const float half_h = (360.0f * 0.5f) / cam.zoom;
                cam.projection_matrix = Math::Ortho(-half_w, half_w, -half_h, half_h, cam.near_plane, cam.far_plane);
            }

            auto& ch = GetMainCameraChannel().get<MainCameraData>();
            ch.view = cam.view_matrix;
            ch.projection = cam.projection_matrix;

            cam.dirty = false;
            tf.dirty = false;
            return;
        }
    }

} // dodoe
