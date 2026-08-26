// do@Redlive

#include "model_importer.h"

#include "runtime/resource/file/file_system.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/function/render/material/material.h"
#include "runtime/resource/resource_manager.h"

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include <filesystem>

namespace dodoe {

    namespace {

        FsPath ResolveTexturePath(const FsPath& model_directory, const aiString& texture_path, const FsPath& asset_dir) {
            FsPath resolved_path = FsPath(texture_path.C_Str());
            if (resolved_path.is_relative()) {
                resolved_path = model_directory / resolved_path;
            }
            resolved_path = resolved_path.lexically_normal();

            if (!asset_dir.empty()) {
                std::error_code ec;
                const FsPath relative_path = std::filesystem::relative(resolved_path, asset_dir, ec);
                const String relative_str = String(relative_path.generic_string().c_str());
                if (!ec && !relative_path.empty() && !relative_str.starts_with("..")) {
                    return relative_path;
                }
            }
            return resolved_path;
        }

        FileID ImportTexture(const FsPath& model_directory, const aiString& texture_path, const FsPath& asset_dir) {
            if (texture_path.length == 0 || texture_path.C_Str()[0] == '*') {
                return FileID();
            }

            const FsPath resolved_path = ResolveTexturePath(model_directory, texture_path, asset_dir);
            return FileID(String(resolved_path.generic_string().c_str()));
        }

        FileID LoadMaterialTexture(
            const aiMaterial* material,
            const FsPath& model_directory,
            const FsPath& asset_dir,
            const aiTextureType primary_type,
            const aiTextureType fallback_type = aiTextureType_NONE) {
            if (!material) {
                return FileID();
            }

            for (const aiTextureType type : {primary_type, fallback_type}) {
                if (type == aiTextureType_NONE || material->GetTextureCount(type) == 0) {
                    continue;
                }

                aiString texture_path{};
                if (material->GetTexture(type, 0, &texture_path) != aiReturn_SUCCESS) {
                    continue;
                }

                const FileID texture_id = ImportTexture(model_directory, texture_path, asset_dir);
                if (texture_id.isValid()) {
                    return texture_id;
                }
            }

            return FileID();
        }

        MaterialProperties MakeMaterial(const aiScene* imported_scene, const aiMesh& source_mesh, const FsPath& model_directory, const FsPath& asset_dir) {
            MaterialProperties material{};

            if (!imported_scene || source_mesh.mMaterialIndex >= imported_scene->mNumMaterials) {
                return material;
            }

            const aiMaterial* source_material = imported_scene->mMaterials[source_mesh.mMaterialIndex];
            if (!source_material) {
                return material;
            }

            aiColor4D base_color{};
            if (aiGetMaterialColor(source_material, AI_MATKEY_BASE_COLOR, &base_color) == aiReturn_SUCCESS ||
                aiGetMaterialColor(source_material, AI_MATKEY_COLOR_DIFFUSE, &base_color) == aiReturn_SUCCESS) {
                material.color = {base_color.r, base_color.g, base_color.b, base_color.a};
            }

            material.base_color_texture = LoadMaterialTexture(
                source_material,
                model_directory,
                asset_dir,
                aiTextureType_BASE_COLOR,
                aiTextureType_DIFFUSE);
            material.normal_texture = LoadMaterialTexture(
                source_material,
                model_directory,
                asset_dir,
                aiTextureType_NORMALS,
                aiTextureType_NORMAL_CAMERA);
            material.emissive_texture = LoadMaterialTexture(
                source_material,
                model_directory,
                asset_dir,
                aiTextureType_EMISSIVE);

            return material;
        }

        PPtr<Texture2D> TexturePtr(const FileID& file_id) {
            PPtr<Texture2D> ptr;
            if (file_id.isValid()) {
                ptr.setLegacyPath(file_id.getPath());
            }
            return ptr;
        }

        void WriteMaterialFile(const FsPath& materials_dir, const String& file_name, const MaterialProperties& props) {
            FsPath absolute_path = materials_dir / file_name;
            Material material;
            material.setColor(props.color);
            material.setEmissive(props.emissive);
            material.setMetallic(props.metallic);
            material.setRoughness(props.roughness);
            material.setBaseColorTexture(TexturePtr(props.base_color_texture));
            material.setNormalTexture(TexturePtr(props.normal_texture));
            material.setMetallicRoughnessTexture(TexturePtr(props.metallic_roughness_texture));
            material.setEmissiveTexture(TexturePtr(props.emissive_texture));
            (void)material.saveToJson(String(absolute_path.generic_string().c_str()));
        }

    } // namespace

    Scope<Asset> ModelImporter::import(const ImportContext& ctx) {
        auto mesh = create_scope<MeshAsset>();
        mesh->setName(FileSystem::PathToNameNoExt(ctx.source_path));

        if (ctx.absolute_source_path.empty()) {
            return mesh;
        }

        Assimp::Importer importer;
        const aiScene* imported_scene = importer.ReadFile(
            ctx.absolute_source_path.c_str(),
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices);

        if (!imported_scene || (imported_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !imported_scene->mRootNode) {
            return mesh;
        }

        AssetManager* asset_manager = ResourceManager::Self().getAssetManager();
        if (!asset_manager) {
            return mesh;
        }

        const FsPath model_directory = FsPath(ctx.absolute_source_path).parent_path();
        const String model_stem = FileSystem::PathToNameNoExt(ctx.source_path);
        const FsPath asset_dir = asset_manager->getAssetDir();
        const FsPath materials_dir = asset_dir / "materials";

        DynamicArray<MaterialProperties> baked_materials{};

        for (UInt32 mesh_index = 0; mesh_index < imported_scene->mNumMeshes; ++mesh_index) {
            const aiMesh* source_mesh = imported_scene->mMeshes[mesh_index];
            if (!source_mesh) {
                continue;
            }

            const MaterialProperties props = MakeMaterial(imported_scene, *source_mesh, model_directory, asset_dir);
            Size_t baked_index = baked_materials.size();
            for (Size_t i = 0; i < baked_materials.size(); ++i) {
                if (baked_materials[i] == props) {
                    baked_index = i;
                    break;
                }
            }

            if (baked_index == baked_materials.size()) {
                baked_materials.push_back(props);
            }
        }

        std::error_code ec;
        std::filesystem::create_directories(materials_dir, ec);

        for (Size_t i = 0; i < baked_materials.size(); ++i) {
            const String file_name = String(fmt::format("{}_{}.domat", model_stem, i).c_str());
            const String source_path = String(("materials/" + file_name).c_str());

            UUID asset_id = asset_manager->registerAsset(source_path, AssetType::Material);
            if (!asset_id.isValid()) {
                continue;
            }
            if (!asset_manager->findAsset(asset_id)) {
                auto mat = create_scope<MaterialAsset>();
                mat->setObjectID(ObjectID{asset_id, 0});
                mat->setName(FileSystem::PathToNameNoExt(source_path));
                AssetMetaData meta;
                meta.ref = ObjectID{asset_id, 0};
                meta.type = AssetType::Material;
                meta.source_file = FileID(source_path);
                meta.source_path = source_path;
                mat->setMetaData(meta);
                asset_manager->registerMaterialAsset(std::move(mat));
            }

            WriteMaterialFile(materials_dir, file_name, baked_materials[i]);
        }

        return mesh;
    }

} // dodoe
