// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/file/file_id.h"
#include "runtime/function/animation/skeleton.h"
#include "runtime/function/animation/anim_clip.h"
#include "runtime/function/render/material/material.h"

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

    struct MeshSection {
        UInt32 vertex_base{0};
        UInt32 vertex_count{0};
        UInt32 index_base{0};
        UInt32 index_count{0};
        UInt32 material_index{0};
        Matrix4f world{1.0f};
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
        DynamicArray<MeshSection> sections;
        DynamicArray<FileID> textures;
        PPtr<Skeleton> skeleton{};
        DynamicArray<PPtr<AnimClip>> animations{};
    };

    struct MeshBlob {
        Ref<MeshData> data{nullptr};
        DynamicArray<MeshNode> hierarchy{};
        DynamicArray<String> material_paths{};

        MeshBlob() = default;
        ~MeshBlob();

        void load(const String& path, const UUID& asset_id);
        void free();

        [[nodiscard]] bool isValid() const { return data != nullptr; }
    };

    [[nodiscard]] String MakeMaterialAssetPath(const String& model_stem, UInt32 material_index);
    [[nodiscard]] Bool BuildMeshImport(
        const String& absolute_source_path,
        const FsPath& asset_dir,
        MeshBlob& out_blob,
        DynamicArray<MaterialProperties>& out_materials);
    [[nodiscard]] Bool SaveMeshCache(const String& absolute_cache_path, const MeshBlob& blob);
    [[nodiscard]] Bool LoadMeshCache(const String& absolute_cache_path, MeshBlob& out_blob);

} // namespace dodoe
