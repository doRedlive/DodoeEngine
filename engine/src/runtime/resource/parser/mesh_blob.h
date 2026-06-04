// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/framework/mesh.h"

namespace dodoe {

    struct MeshBlob {
        Ref<MeshData> data{nullptr};

        MeshBlob() = default;
        explicit MeshBlob(const std::string& path);
        ~MeshBlob();

        void load(const std::string& path);
        void free();

        [[nodiscard]] bool isValid() const { return data != nullptr; }
    };

} // dodoe
