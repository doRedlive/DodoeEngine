// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/file/file_id.h"

namespace dodoe {

    class Material {
    public:
        Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
        Vector3f emissive{0.0f, 0.0f, 0.0f};
        float metallic{0.0f};
        float roughness{1.0f};
        FileID base_color_texture{};
        FileID normal_texture{};
        FileID metallic_roughness_texture{};
        FileID emissive_texture{};
    };


} // dodoe