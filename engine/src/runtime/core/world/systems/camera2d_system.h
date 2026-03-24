//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_CAMERA2D_SYSTEM_H
#define DODOE_CAMERA2D_SYSTEM_H

#include "dopch.h"

#include "system.h"

#include "../components.h"
#include "../components/camera2d_component.h"

#include "runtime/core/utils/tags.h"

namespace dodoe {

    namespace system {

        using namespace component;

        class Camera2dSystem : public System {
        public:
            ~Camera2dSystem() override = default;

            void update(Registry& reg, float dt) override {
                auto view = reg.view<Camera2dComponent, TransformComponent, TagComponent>();
                for (auto entity : view) {
                    auto tag = reg.get<TagComponent>(entity).id;
                    if (tag != tag::PrimaryCameraTag) {
                        continue;
                    }

                    auto& camera_comp = reg.get<Camera2dComponent>(entity);
                    auto& transform_comp = reg.get<TransformComponent>(entity);
                    auto& camera = context.camera();
                    if (camera_comp.dirty) {
                        camera.set_position(transform_comp.position);
                        camera.set_rotation(transform_comp.rotation);
                        camera.set_zoom(camera_comp.zoom);
                        camera.set_clear_color(camera_comp.background_color);
                    }
                }
            }
        };

    } // system

} // dodoe

#endif//DODOE_CAMERA2D_SYSTEM_H