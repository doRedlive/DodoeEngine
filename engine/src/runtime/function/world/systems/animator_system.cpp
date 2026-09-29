// do@Redlive

#include "animator_system.h"

#include "runtime/function/animation/animator_controller.h"
#include "runtime/function/animation/anim_clip.h"
#include "runtime/function/animation/skeleton.h"
#include "runtime/function/render/pixel2d/sprite.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/asset/types/mesh_asset.h"

namespace dodoe {

    AnimatorSystem::~AnimatorSystem() = default;

    SystemAccess AnimatorSystem::getAccess() const {
        return SystemAccessBuilder{}
            .readsComponents<AnimatorComponent, SpriteRendererComponent, MeshRendererComponent, AnimationPoseComponent,
                             PlayAnimationRequest, StopAnimationRequest, ResumeAnimationRequest,
                             ActiveComponent, HierarchyComponent>()
            .writesComponents<AnimatorComponent, SpriteRendererComponent, MeshRendererComponent, AnimationPoseComponent,
                             PlayAnimationRequest, StopAnimationRequest, ResumeAnimationRequest>()
            .hasStructuralChanges(true)
            .build();
    }

    Float AnimatorSystem::parameterDefault(const AnimatorParameter& parameter) {
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

    Bool AnimatorSystem::evaluateCondition(AnimatorComponent& animator,
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
                value = parameterDefault(*parameter);
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

    void AnimatorSystem::fireClipEvents(AnimatorComponent& animator,
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

    void AnimatorSystem::applyPlayRequest(AnimatorComponent& animator, const String& state_name) {
        if (!animator.controller) {
            return;
        }
        const Size_t index = animator.controller->findState(state_name);
        if (index == AnimatorController::kInvalidState) {
            return;
        }
        animator.cur_state = index;
        animator.state_time = 0.0f;
        animator.prev_state_time = 0.0f;
        animator.cur_frame_id = 0;
        animator.applied_frame_id = static_cast<Size_t>(-1);
        animator.playing = true;
    }

    const AnimClip* AnimatorSystem::resolveClipByPath(const String& path_with_clip) {
        if (path_with_clip.empty()) {
            return nullptr;
        }
        AssetManager* asset_manager = ResourceManager::Self().getAssetManager();
        if (!asset_manager) {
            return nullptr;
        }

        String file_path = path_with_clip;
        String clip_name{};
        const Size_t separator = file_path.find('#');
        if (separator != String::npos) {
            clip_name = file_path.substr(separator + 1);
            file_path.resize(separator);
        }

        const ObjectID ref = asset_manager->resolvePathToRef(FileID(file_path));
        if (!ref.isValid()) {
            return nullptr;
        }
        MeshAsset* mesh_asset = asset_manager->loadAssetSync<MeshAsset>(ref.asset_id);
        if (!mesh_asset) {
            return nullptr;
        }

        const DynamicArray<MeshAnimClipData>& clips = mesh_asset->getClips();
        if (clips.empty()) {
            return nullptr;
        }

        Size_t clip_index = 0;
        if (!clip_name.empty()) {
            Bool found = false;
            for (Size_t i = 0; i < clips.size(); ++i) {
                if (clips[i].name == clip_name) {
                    clip_index = i;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return nullptr;
            }
        }

        return ResourceManager::Self().loadObject<AnimClip>(ref.asset_id, AnimClip::kLocalIdBase + static_cast<UInt32>(clip_index));
    }

    void AnimatorSystem::evaluateTransitions(AnimatorComponent& animator,
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
                    if (!evaluateCondition(animator, controller, condition)) {
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

    void AnimatorSystem::update(Registry& reg, float dt) {
        static const DynamicArray<AnimFrame2D> k_empty_frames{};

        auto view = reg.view<AnimatorComponent>();
        for (auto entity : view) {
            if (!entity.activeInHierarchy()) {
                continue;
            }
            auto& animator = reg.get<AnimatorComponent>(entity);

            if (reg.all_of<PlayAnimationRequest>(entity)) {
                const auto& request = reg.get<PlayAnimationRequest>(entity);
                applyPlayRequest(animator, request.name);
            }
            if (!animator.play_request.empty()) {
                applyPlayRequest(animator, animator.play_request);
                animator.play_request.clear();
            }
            if (reg.all_of<StopAnimationRequest>(entity) || animator.stop_requested) {
                animator.playing = false;
            }
            animator.stop_requested = false;
            if (reg.all_of<ResumeAnimationRequest>(entity) || animator.resume_requested) {
                animator.playing = true;
            }
            animator.resume_requested = false;

            animator.pending_events.clear();

            if (!animator.controller.get() && animator.controller.getObjectID().isValid()) {
                const ObjectID& ref = animator.controller.getObjectID();
                if (AnimatorController* resolved =
                        ResourceManager::Self().loadObject<AnimatorController>(ref.asset_id, ref.local_id)) {
                    animator.controller = PPtr<AnimatorController>(resolved);
                }
            }

            if (animator.play_on_awake && !animator.playing && !animator.auto_played && animator.controller) {
                animator.playing = true;
                animator.auto_played = true;
            }

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
                const Anim2DClip* clip = clip_ref.clip_2d.get();
                const DynamicArray<AnimFrame2D>& frames = clip ? clip->getFrames() : k_empty_frames;
                if (!clip || frames.empty()) {
                    continue;
                }
                const Float total_ms = clip->totalDurationMs();
                if (total_ms <= 0.0f) {
                    continue;
                }

                animator.prev_state_time = animator.state_time;
                animator.state_time += dt * 1000.0f * animator.speed * state.speed;

                Size_t frame_id = animator.cur_frame_id;
                if (frame_id >= frames.size()) {
                    frame_id = 0;
                }

                Float time_ms = animator.state_time;
                while (time_ms >= frames[frame_id].duration) {
                    if (frames[frame_id].duration > 0.0f) {
                        time_ms -= frames[frame_id].duration;
                    }
                    frame_id += 1;
                    if (frame_id >= frames.size()) {
                        if (clip->getLoop()) {
                            frame_id = 0;
                        }
                        else {
                            frame_id = frames.size() - 1;
                            time_ms = 0.0f;
                            break;
                        }
                    }
                }
                animator.cur_frame_id = frame_id;
                animator.state_time = time_ms;

                fireClipEvents(animator, clip->getEvents(), total_ms);
                evaluateTransitions(animator, *controller, total_ms);

                const auto& frame = frames[animator.cur_frame_id];
                if (frame.texture.isValid() &&
                    animator.applied_frame_id != animator.cur_frame_id &&
                    entity.hasComponent<SpriteRendererComponent>()) {
                    auto* tex = frame.texture.get();
                    if (tex) {
                        auto& sprite_renderer = reg.get<SpriteRendererComponent>(entity);
                        sprite_renderer.sprite = PPtr<Sprite>(ResourceManager::Self().loadObjectByPath<Sprite>(FileID(tex->getPath())));
                        animator.applied_frame_id = animator.cur_frame_id;
                    }
                }
            }
            else if (clip_ref.type == AnimatorClipType::Clip3D) {
                const AnimClip* clip = clip_ref.clip_3d.get();
                if (!clip && clip_ref.clip_3d.isAssigned()) {
                    const ObjectID& clip_id = clip_ref.clip_3d.getObjectID();
                    clip = ResourceManager::Self().loadObject<AnimClip>(clip_id.asset_id, clip_id.local_id);
                }
                if (!clip && !clip_ref.clip_3d.getLegacyPath().empty()) {
                    clip = resolveClipByPath(clip_ref.clip_3d.getLegacyPath());
                }
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

                fireClipEvents(animator, clip->events, total_ms);
                evaluateTransitions(animator, *controller, total_ms);

                Float sample_time = animator.state_time / 1000.0f;
                if (state.loop) {
                    sample_time = glm::mod(sample_time, clip->duration);
                }
                else {
                    sample_time = Math::Clamp(sample_time, 0.0f, clip->duration);
                }

                DynamicArray<BoneBindPose> local_poses;
                DynamicArray<Matrix4f> world_matrices;
                DynamicArray<Matrix4f> skinning_matrices;
                clip->sample(*mesh_renderer.skeleton, sample_time, local_poses);
                mesh_renderer.skeleton->computeWorldMatrices(local_poses, world_matrices);
                mesh_renderer.skeleton->computeSkinningMatrices(world_matrices, skinning_matrices);
                mesh_renderer.skinning_matrices.swap(skinning_matrices);

                auto& pose = reg.get_or_emplace<AnimationPoseComponent>(entity);
                pose.local_poses = std::move(local_poses);
                pose.world_matrices = std::move(world_matrices);
                pose.skinning_matrices = mesh_renderer.skinning_matrices;
                pose.dirty = true;
            }
        }

        reg.raw().clear<PlayAnimationRequest>();
        reg.raw().clear<StopAnimationRequest>();
        reg.raw().clear<ResumeAnimationRequest>();
    }

} // dodoe
