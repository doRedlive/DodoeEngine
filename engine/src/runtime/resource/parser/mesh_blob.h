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

    struct MeshNode {
        String name{};
        Vector3f position{0.0f};
        Vector3f rotation{0.0f};
        Vector3f scale{1.0f};
        Int32 parent_index{-1};
        Int32 mesh_section_index{-1};
    };

    struct MeshData {
        DynamicArray<MeshVertex> vertices;
        DynamicArray<UInt32> indices;
        DynamicArray<FileID> textures;
        PPtr<Skeleton> skeleton{};
        DynamicArray<PPtr<AnimClip>> animations{};
    };

    struct MeshBlob {
        Ref<MeshData> data{nullptr};
        DynamicArray<MeshNode> hierarchy{};

        MeshBlob() = default;
        ~MeshBlob();

        void load(const String& path, const UUID& asset_id);
        void free();

        [[nodiscard]] bool isValid() const { return data != nullptr; }
    };

} // namespace dodoe
