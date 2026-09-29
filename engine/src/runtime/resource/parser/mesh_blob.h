// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/file/file_id.h"
#include "runtime/function/animation/skeleton.h"
#include "runtime/function/animation/anim_clip.h"
#include "runtime/function/render/material/material.h"

struct aiMesh;
struct aiNode;
struct aiScene;

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

    struct MeshAnimClipData {
        String name{};
        Float duration{0.0f};
        Bool loop{true};
        DynamicArray<AnimBoneChannel3D> channels{};
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
        DynamicArray<SkeletonNode> skeleton_nodes{};
        DynamicArray<MeshAnimClipData> clips{};

        MeshBlob() = default;
        ~MeshBlob();

        void load(const String& path, const UUID& asset_id);
        void free();

        [[nodiscard]] bool isValid() const { return data != nullptr; }
        [[nodiscard]] bool isSkinned() const { return !skeleton_nodes.empty(); }

        [[nodiscard]] static Bool BuildMeshImport(
            const String& absolute_source_path,
            const FsPath& asset_dir,
            MeshBlob& out_blob,
            DynamicArray<MaterialProperties>& out_materials);

    private:
        struct VertexBoneData {
            static constexpr UInt32 kMaxInfluences = 4;

            UInt32 ids[kMaxInfluences]{0, 0, 0, 0};
            Float weights[kMaxInfluences]{0.0f, 0.0f, 0.0f, 0.0f};

            void add(UInt32 bone_index, Float weight);
            void normalize();
        };

        static MeshVertex MakeMeshVertex(const aiMesh& mesh, unsigned int vertex_index, const DynamicArray<VertexBoneData>& bone_data);
        static void BuildHierarchyNode(const aiNode& node, Int32 parent_index, DynamicArray<MeshNode>& out);
        static void CollectSectionWorlds(const aiNode& node, const Matrix4f& parent_world, UnorderedMap<UInt32, Matrix4f>& out_worlds);
        [[nodiscard]] static MaterialProperties MakeMaterial(const aiScene* imported_scene, const aiMesh& source_mesh, const FsPath& model_directory, const FsPath& asset_dir);
        static Int32 BuildSkeletonNodes(const aiNode& node, Int32 parent_index, const UnorderedSet<String>& bone_names, DynamicArray<SkeletonNode>& out_nodes);
        static void ImportAnimations(const aiScene& scene, const UnorderedMap<String, Int32>& node_indices, DynamicArray<MeshAnimClipData>& out_clips);
        void bindSkeletalObjects(const UUID& asset_id);
    };

    [[nodiscard]] String MakeMaterialAssetPath(const String& model_stem, UInt32 material_index);
    [[nodiscard]] Bool SaveMeshCache(const String& absolute_cache_path, const MeshBlob& blob);
    [[nodiscard]] Bool LoadMeshCache(const String& absolute_cache_path, MeshBlob& out_blob);

    void PackVertexBytes(const DynamicArray<MeshVertex>& vertices,
                         DynamicArray<UInt8>& out_bytes);

} // namespace dodoe
