// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/file/file_id.h"
#include "runtime/function/animation/skeleton.h"
#include "runtime/function/animation/anim_clip.h"

namespace dodoe {

    struct MeshVertex {
        Vector3f position;
        Vector3f normal;
        Vector2f tex_coords;
        Vector3f tangent;
        Vector3f bitangent;
        UInt32 bone_ids[4]{0, 0, 0, 0};
        Float bone_weights[4]{0.0f, 0.0f, 0.0f, 0.0f};
    };

    struct MeshData {
        DynamicArray<MeshVertex> vertices;
        DynamicArray<UInt32> indices;
        DynamicArray<FileID> textures;
        Ref<Skeleton> skeleton{nullptr};
        DynamicArray<Ref<AnimClip>> animations{};
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
