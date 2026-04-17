// do->GreenMuffin

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/core/utils/util.h"

REFLECTION_TYPE(ModelRendererComponent)
REFLECTION_TYPE(MeshRendererComponent)

namespace dodoe {

    STRUCT(ModelRendererComponent, WhiteListFields) {
        REFLECTION_BODY(ModelRendererComponent)

        META(Enable)
        identifier model_id{ 0 };
        META(Enable)
        Color color{ };
        ModelRendererComponent() = default;
    };

    STRUCT(MeshRendererComponent, WhiteListFields) {
        REFLECTION_BODY(MeshRendererComponent)

        META(Enable)
        identifier mesh_id;
        
    };

} // dodoe