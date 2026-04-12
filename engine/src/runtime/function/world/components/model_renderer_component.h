// do->GreenMuffin

#pragma once

#include "dopch.h"

#include "runtime/core/utils/uuid.h"
#include "runtime/core/utils/util.h"

namespace dodoe {

    struct ModelRendererComponent {
        identifier model_id{ 0 };
        Color color{ };
        ModelRendererComponent() = default;
    };

    struct MeshRendererComponent {
        identifier mesh_id;
        
    };

} // dodoe