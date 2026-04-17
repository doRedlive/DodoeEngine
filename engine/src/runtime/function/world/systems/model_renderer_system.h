#ifndef DODOE_MODEL_RENDERER_SYSTEM_H
#define DODOE_MODEL_RENDERER_SYSTEM_H

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/function/render/render_resource.h"

#include "glm/gtc/matrix_transform.hpp"

namespace dodoe {

    namespace {
        inline Matrix4f BuildModelMatrix(const TransformComponent& transform) {
            Matrix4f model(1.0f);
            model = glm::translate(model, transform.position);
            model = glm::rotate(model, glm::radians(transform.rotation.x), Vector3f(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, glm::radians(transform.rotation.y), Vector3f(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(transform.rotation.z), Vector3f(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, transform.scale);
            return model;
        }
    }

    class ModelRendererSystem : public System {
    public:
        ~ModelRendererSystem() override = default;

        void update(Registry& reg, float dt) override {
            (void)dt;
            auto view = reg.view<ModelRendererComponent, TransformComponent>();
            for (auto entity : view) {
                auto& renderer = reg.get<ModelRendererComponent>(entity);
                auto& transform = reg.get<TransformComponent>(entity);
                if (renderer.model_id == 0) {
                    continue;
                }

                MainCameraMeshSubmitData submit_data{};
                submit_data.entity_id = static_cast<identifier>(entity.handle());
                submit_data.model_id = renderer.model_id;
                submit_data.model_matrix = BuildModelMatrix(transform);
                submit_data.color = renderer.color.to_vec4();
                g_RenderResource->submitMainCamera(submit_data);
            }
        }
    };

} // dodoe

#endif//DODOE_MODEL_RENDERER_SYSTEM_H
