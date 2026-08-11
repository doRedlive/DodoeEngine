// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/uuid.h"

namespace dodoe {

    struct MaterialProperties {
        Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
        Vector3f emissive{0.0f, 0.0f, 0.0f};
        Float metallic{0.0f};
        Float roughness{1.0f};
        UUID base_color_texture{};
        UUID normal_texture{};
        UUID metallic_roughness_texture{};
        UUID emissive_texture{};
        Bool operator==(const MaterialProperties& other) const = default;
    };

} // dodoe
