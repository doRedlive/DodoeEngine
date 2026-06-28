// do@Redlive

#include "camera_system.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/math/math.h"
#include "runtime/core/utils/tags.h"
#include "runtime/function/render/render_view/render_view.h"

namespace dodoe {

    CameraSystem::~CameraSystem() = default;

    void CameraSystem::update(Registry& reg, float dt) {
        (void)dt;

        auto* rs = GetRenderSystem();
        if (!rs) return;

        auto view = reg.view<Camera2dComponent, TransformComponent, TagComponent>();
        for (auto entity : view) {
            const auto tag_id = reg.get<TagComponent>(entity).id;
            if (tag_id != tag::PrimaryCameraTag) continue;

            auto& cam = reg.get<Camera2dComponent>(entity);
            auto& tf  = reg.get<TransformComponent>(entity);
            if (!cam.dirty && !tf.dirty) continue;

            Matrix4f view_matrix(1.0f);
            Matrix4f projection_matrix(1.0f);
            const auto pos = tf.position;
            const auto rot = tf.rotation;

            if (cam.type == CameraType::Perspective) {
                const Vector3f eye(pos.x, pos.y, pos.z);
                const Vector3f center(pos.x + Math::Sin(Math::Radians(rot.y)) * Math::Cos(Math::Radians(rot.x)),
                                      pos.y + Math::Sin(Math::Radians(rot.x)),
                                      pos.z - Math::Cos(Math::Radians(rot.y)) * Math::Cos(Math::Radians(rot.x)));
                const Vector3f up(0.0f, 1.0f, 0.0f);
                view_matrix = Math::LookAt(eye, center, up);
                const auto* vp = rs->getViewportManager();
                const auto vs = vp ? vp->getPixelSize() : Vector2i(640, 360);
                const float aspect = static_cast<float>(vs.x) / static_cast<float>(std::max(vs.y, 1));
                projection_matrix = Math::Perspective(Math::Radians(cam.fov), aspect, cam.near_plane, cam.far_plane);
            } else {
                const Vector3f eye(pos.x, pos.y, 10.0f);
                const Vector3f center(pos.x, pos.y, 0.0f);
                view_matrix = Math::LookAt(eye, center, Vector3f(0.0f, 1.0f, 0.0f));
                const auto* vp = rs->getViewportManager();
                const auto vs = vp ? vp->getPixelSize() : Vector2i(640, 360);
                const float half_w = vs.x * 0.5f / cam.zoom;
                const float half_h = vs.y * 0.5f / cam.zoom;
                projection_matrix = Math::Ortho(-half_w, half_w, -half_h, half_h, cam.near_plane, cam.far_plane);
            }

            const Matrix4f vp_matrix = projection_matrix * view_matrix;

            auto* vf = rs->getViewFamily();
            vf->reset();
            RenderView rv(Identifier{});
            const auto* vs = rs->getViewportManager();
            const auto pixel_size = vs ? vs->getPixelSize() : Vector2i(640, 360);
            rv.setViewportRect(Vector4i(0, 0, pixel_size.x, pixel_size.y));
            rv.setMatrices(view_matrix, projection_matrix);
            vf->addView(rv);

            cam.dirty = false;
            tf.dirty = false;
            return;
        }
    }

} // dodoe
