// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/math/math.h"
#include "runtime/function/animation/skeleton.h"

namespace dodoe {

    struct AnimationPoseComponent {
        DynamicArray<BoneBindPose> local_poses{};
        DynamicArray<Matrix4f> world_matrices{};
        DynamicArray<Matrix4f> skinning_matrices{};
        bool dirty{false};
    };

} // dodoe
