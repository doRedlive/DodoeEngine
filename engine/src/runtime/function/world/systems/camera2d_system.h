//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_CAMERA2D_SYSTEM_H
#define DODOE_CAMERA2D_SYSTEM_H

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"

#include "runtime/core/utils/tags.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_system.h"

namespace dodoe {

    class Camera2dSystem : public System {
    public:
        ~Camera2dSystem() override = default;

        void update(Registry& reg, float dt) override {
            (void)dt;
            auto view = reg.view<Camera2dComponent, TransformComponent, TagComponent>();
            for (auto entity : view) {
                auto tag = reg.get<TagComponent>(entity).id;
                if (tag != tag::PrimaryCameraTag) {
                    continue;
                }

                auto& camera_comp = reg.get<Camera2dComponent>(entity);
                auto& transform_comp = reg.get<TransformComponent>(entity);
                auto& camera = Application::self().context().render_system->camera();
                if (camera_comp.dirty) {
                    camera.set_position(transform_comp.position);
                    camera.set_rotation(transform_comp.rotation.z);
                    camera.set_zoom(camera_comp.zoom);
                    camera.set_clear_color(camera_comp.background);
                }
            }
        }
    };

} // dodoe

#endif//DODOE_CAMERA2D_SYSTEM_H
