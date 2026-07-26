// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/file/file_id.h"

namespace dodoe {

    struct MeshVertex {
        Vector3f position;
        Vector3f normal;
        Vector2f tex_coords;
        Vector3f tangent;
        Vector3f bitangent;
    };

    struct MeshData {
        DynamicArray<MeshVertex> vertices;
        DynamicArray<UInt32> indices;
        DynamicArray<FileID> textures;
    };

    struct MeshBlob {
        Ref<MeshData> data{nullptr};

        MeshBlob() = default;
        explicit MeshBlob(const String& path);
        ~MeshBlob();

        void load(const String& path);
        void free();

        [[nodiscard]] bool isValid() const { return data != nullptr; }
    };

} // namespace dodoe
