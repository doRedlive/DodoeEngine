// do@Redlive

#pragma once

#include "dopch.h"

#include "skeleton.h"
#include "anim2d_clip.h"

namespace dodoe {

    struct AnimBoneChannel3D {
        static constexpr Int32 kInvalidBone = -1;

        Int32 bone{kInvalidBone};
        DynamicArray<Float> position_times{};
        DynamicArray<Vector3f> positions{};
        DynamicArray<Float> rotation_times{};
        DynamicArray<Quaternion> rotations{};
        DynamicArray<Float> scale_times{};
        DynamicArray<Vector3f> scales{};

        void sample(const Float time, BoneBindPose& out_pose) const {
            if (!position_times.empty()) {
                out_pose.position = SampleTrack(position_times, positions, time);
            }
            if (!rotation_times.empty()) {
                out_pose.rotation = SampleTrack(rotation_times, rotations, time);
            }
            if (!scale_times.empty()) {
                out_pose.scale = SampleTrack(scale_times, scales, time);
            }
        }

    private:
        template <typename T>
        static T SampleTrack(const DynamicArray<Float>& times,
                             const DynamicArray<T>& values,
                             const Float time) {
            if (times.empty() || values.empty() || times.size() != values.size()) {
                return T{};
            }
            if (times.size() == 1 || time <= times.front()) {
                return values.front();
            }
            if (time >= times.back()) {
                return values.back();
            }
            Size_t index = 0;
            for (Size_t i = 0; i + 1 < times.size(); ++i) {
                if (time < times[i + 1]) {
                    index = i;
                    break;
                }
            }
            const Float range = times[index + 1] - times[index];
            const Float factor = range > 0.0f ? (time - times[index]) / range : 0.0f;
            if constexpr (std::is_same_v<T, Quaternion>) {
                return Math::Normalize(glm::slerp(values[index], values[index + 1], factor));
            }
            return glm::mix(values[index], values[index + 1], factor);
        }
    };

    class DODOE_API AnimClip : public Object {
    public:
        static constexpr UInt32 kLocalIdBase = 2;

        String name{};
        Float duration{0.0f};
        Bool loop{false};
        DynamicArray<AnimBoneChannel3D> channels{};
        DynamicArray<AnimClipEvent> events{};

        AnimClip() = default;
        explicit AnimClip(const ObjectID& id)
            : Object(id) {}

        [[nodiscard]] const char* getObjectTypeName() const override { return "AnimClip"; }

        void clear() {
            name.clear();
            duration = 0.0f;
            loop = false;
            channels.clear();
            events.clear();
        }

        void sample(const Skeleton& skeleton,
                    const Float time,
                    DynamicArray<BoneBindPose>& out_local_poses) const {
            out_local_poses.resize(skeleton.getNodeCount());
            for (Size_t i = 0; i < skeleton.getNodeCount(); ++i) {
                out_local_poses[i] = skeleton.getNode(static_cast<Int32>(i)).bind_pose;
            }
            for (const auto& channel : channels) {
                if (channel.bone < 0 || channel.bone >= static_cast<Int32>(out_local_poses.size())) {
                    continue;
                }
                channel.sample(time, out_local_poses[static_cast<Size_t>(channel.bone)]);
            }
        }

        [[nodiscard]] static AnimClip* Create(const ObjectID& id);
        static void Shutdown();
    };

} // dodoe
