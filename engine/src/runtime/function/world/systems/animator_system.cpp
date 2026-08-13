// do@Redlive

#include "animator_system.h"

#include "runtime/function/animation/animator_controller.h"
#include "runtime/function/animation/anim_clip.h"
#include "runtime/service/sprite/sprite_loader.h"

namespace dodoe {

    namespace {

        Float ParameterDefault(const AnimatorParameter& parameter) {
            switch (parameter.type) {
                case AnimatorParameterType::Int:
                    return static_cast<Float>(parameter.default_int);
                case AnimatorParameterType::Bool:
                    return parameter.default_bool ? 1.0f : 0.0f;
                case AnimatorParameterType::Trigger:
                    return 0.0f;
                case AnimatorParameterType::Float:
                default:
                    return parameter.default_float;
            }
        }

        Bool EvaluateCondition(AnimatorComponent& animator,
                               const AnimatorController& controller,
                               const AnimatorCondition& condition) {
            Float value = 0.0f;
            const auto it = animator.parameters.find(condition.parameter);
            if (it != animator.parameters.end()) {
                value = it->second;
            }
            else {
                const auto* parameter = controller.findParameter(condition.parameter);
                if (parameter) {
                    value = ParameterDefault(*parameter);
                }
            }

            switch (condition.mode) {
                case AnimatorConditionMode::If:
                    return value != 0.0f;
                case AnimatorConditionMode::IfNot:
                    return value == 0.0f;
                case AnimatorConditionMode::Equals:
                    return value == condition.threshold;
                case AnimatorConditionMode::NotEqual:
                    return value != condition.threshold;
                case AnimatorConditionMode::Less:
                    return value < condition.threshold;
                case AnimatorConditionMode::Greater:
                default:
                    return value > condition.threshold;
            }
        }

        void FireClipEvents(AnimatorComponent& animator,
                            const DynamicArray<AnimClipEvent>& events,
                            const Float total_ms) {
            for (const auto& event : events) {
                const Float event_ms = event.time * total_ms;
                Bool fired = false;
                if (animator.state_time >= animator.prev_state_time) {
                    fired = event_ms > animator.prev_state_time && event_ms <= animator.state_time;
                }
                else {
                    fired = event_ms > animator.prev_state_time || event_ms <= animator.state_time;
                }
                if (fired) {
                    animator.pending_events.push_back(event.function_name);
                }
            }
        }

        void EvaluateTransitions(AnimatorComponent& animator,
                                 const AnimatorController& controller,
                                 const Float total_ms) {
            for (Size_t ti = 0; ti < controller.getTransitionCount(); ++ti) {
                const auto& transition = controller.getTransition(ti);
                if (transition.from_state != animator.cur_state) {
                    continue;
                }

                Bool should_transition = false;
                if (!transition.conditions.empty()) {
                    Bool all_ok = true;
                    for (const auto& condition : transition.conditions) {
                        if (!EvaluateCondition(animator, controller, condition)) {
                            all_ok = false;
                            break;
                        }
                    }
                    should_transition = all_ok;
                }
                else if (transition.has_exit_time) {
                    should_transition = animator.state_time / total_ms >= transition.exit_time;
                }
                else {
                    should_transition = true;
                }

                if (should_transition) {
                    for (const auto& condition : transition.conditions) {
                        const auto* parameter = controller.findParameter(condition.parameter);
                        if (parameter && parameter->type == AnimatorParameterType::Trigger) {
                            animator.parameters[condition.parameter] = 0.0f;
                        }
                    }
                    animator.cur_state = transition.to_state;
                    animator.state_time = 0.0f;
                    animator.prev_state_time = 0.0f;
                    animator.cur_frame_id = 0;
                    animator.applied_frame_id = static_cast<Size_t>(-1);
                    break;
                }
            }
        }

    } // anonymous namespace

    AnimatorSystem::~AnimatorSystem() = default;

    SystemAccess AnimatorSystem::getAccess() const {
        return SystemAccessBuilder{}
            .readsComponents<AnimatorComponent, SpriteRendererComponent, MeshRendererComponent>()
            .writesComponents<AnimatorComponent, SpriteRendererComponent, MeshRendererComponent>()
            .build();
    }

    void AnimatorSystem::update(Registry& reg, float dt) {
        auto view = reg.view<AnimatorComponent>();
        for (auto entity : view) {
            auto& animator = reg.get<AnimatorComponent>(entity);

            if (!animator.play_request.empty()) {
                if (animator.controller) {
                    const Size_t index = animator.controller->findState(animator.play_request);
                    if (index != AnimatorController::kInvalidState) {
                        animator.cur_state = index;
                        animator.state_time = 0.0f;
                        animator.prev_state_time = 0.0f;
                        animator.cur_frame_id = 0;
                        animator.applied_frame_id = static_cast<Size_t>(-1);
                        animator.playing = true;
                    }
                }
                animator.play_request.clear();
            }
            if (animator.stop_requested) {
                animator.playing = false;
                animator.stop_requested = false;
            }
            if (animator.resume_requested) {
                animator.playing = true;
                animator.resume_requested = false;
            }

            animator.pending_events.clear();

            if (!animator.playing || !animator.controller) {
                continue;
            }
            auto* controller = animator.controller.get();
            if (animator.cur_state >= controller->getStateCount()) {
                continue;
            }

            const auto& state = controller->getState(animator.cur_state);
            const AnimatorClipRef& clip_ref = state.clip;

            if (clip_ref.type == AnimatorClipType::Clip2D) {
                const AnimClip2D* clip = clip_ref.clip_2d.get();
                if (!clip || clip->frames.empty()) {
                    continue;
                }
                const Float total_ms = clip->totalDurationMs();
                if (total_ms <= 0.0f) {
                    continue;
                }

                animator.prev_state_time = animator.state_time;
                animator.state_time += dt * 1000.0f * animator.speed * state.speed;

                Size_t frame_id = animator.cur_frame_id;
                if (frame_id >= clip->frames.size()) {
                    frame_id = 0;
                }

                Float time_ms = animator.state_time;
                while (time_ms >= clip->frames[frame_id].duration) {
                    if (clip->frames[frame_id].duration > 0.0f) {
                        time_ms -= clip->frames[frame_id].duration;
                    }
                    frame_id += 1;
                    if (frame_id >= clip->frames.size()) {
                        if (clip->loop) {
                            frame_id = 0;
                        }
                        else {
                            frame_id = clip->frames.size() - 1;
                            time_ms = 0.0f;
                            break;
                        }
                    }
                }
                animator.cur_frame_id = frame_id;
                animator.state_time = time_ms;

                FireClipEvents(animator, clip->events, total_ms);
                EvaluateTransitions(animator, *controller, total_ms);

                const auto& frame = clip->frames[animator.cur_frame_id];
                if (frame.texture_id != 0 &&
                    animator.applied_frame_id != animator.cur_frame_id &&
                    entity.hasComponent<SpriteRendererComponent>()) {
                    auto* tex = static_cast<Texture2D*>(Object::FindObjectFromInstanceID(frame.texture_id));
                    if (tex) {
                        auto& sprite_renderer = reg.get<SpriteRendererComponent>(entity);
                        sprite_renderer.sprite = PPtr<Sprite>(SpriteLoader::Load(tex->getPath()));
                        animator.applied_frame_id = animator.cur_frame_id;
                    }
                }
            }
            else if (clip_ref.type == AnimatorClipType::Clip3D) {
                const AnimClip* clip = clip_ref.clip_3d.get();
                if (!clip || !entity.hasComponent<MeshRendererComponent>()) {
                    continue;
                }
                auto& mesh_renderer = reg.get<MeshRendererComponent>(entity);
                if (!mesh_renderer.skeleton) {
                    continue;
                }
                const Float total_ms = clip->duration * 1000.0f;
                if (total_ms <= 0.0f) {
                    continue;
                }

                animator.prev_state_time = animator.state_time;
                animator.state_time += dt * 1000.0f * animator.speed * state.speed;

                FireClipEvents(animator, clip->events, total_ms);
                EvaluateTransitions(animator, *controller, total_ms);

                Float sample_time = animator.state_time / 1000.0f;
                if (state.loop) {
                    sample_time = glm::mod(sample_time, clip->duration);
                }
                else {
                    sample_time = Math::Clamp(sample_time, 0.0f, clip->duration);
                }

                DynamicArray<BoneBindPose> local_poses;
                DynamicArray<Matrix4f> world_matrices;
                clip->sample(*mesh_renderer.skeleton, sample_time, local_poses);
                mesh_renderer.skeleton->computeWorldMatrices(local_poses, world_matrices);
                mesh_renderer.skeleton->computeSkinningMatrices(world_matrices, mesh_renderer.skinning_matrices);
            }
        }
    }

} // dodoe
