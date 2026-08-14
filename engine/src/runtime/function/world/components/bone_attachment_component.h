// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/math/math.h"

REFLECTION_TYPE(BoneAttachmentComponent)

namespace dodoe {
    STRUCT(BoneAttachmentComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(BoneAttachmentComponent)

        META(Enable)
        String bone_name{};
        META(Enable)
        Vector3f local_offset{};
        META(Enable)
        bool follow_rotation{ true };
    };

} // dodoe
