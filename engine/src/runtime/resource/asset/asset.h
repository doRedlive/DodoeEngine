// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/uuid.h"

REFLECTION_TYPE(AssetRef)

namespace dodoe {

    using AssetHandle = UUID;

    enum class AssetType : ui16 {
        None = 0,
        Scene,
        Texture,
        Model,
        Shader,
    };

    STRUCT(AssetRef, WhiteListFields) {
        REFLECTION_BODY(AssetRef)

        META(Enable)
        AssetHandle handle;
        META(Enable)
        AssetType type;
        META(Enable)
        std::string path;
        META(Enable)
        identifier path_id;

        [[nodiscard]] bool isValid() const { return handle.isValid(); }
    };

} // dodoe


