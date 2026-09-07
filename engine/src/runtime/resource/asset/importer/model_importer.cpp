// do@Redlive

#include "model_importer.h"

#include "runtime/resource/file/file_system.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/asset/asset_database.h"
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

        void WriteMaterialFile(const FsPath& materials_dir, const String& file_name, const MaterialProperties& props, const UnorderedMap<String, ObjectID>& resolved_textures) {
            FsPath absolute_path = materials_dir / file_name;
            Material material;
            material.setColor(props.color);
            material.setEmissive(props.emissive);
            material.setMetallic(props.metallic);
            material.setRoughness(props.roughness);

            auto texture_ptr = [&](const FileID& file_id) {
                PPtr<Texture2D> ptr;
                if (file_id.isValid()) {
                    ptr.setLegacyPath(file_id.getPath());
                    const auto it = resolved_textures.find(file_id.getPath());
                    if (it != resolved_textures.end() && it->second.isValid()) {
                        ptr.setIdentity(it->second, 0);
                    }
                }
                return ptr;
            };

            material.setBaseColorTexture(texture_ptr(props.base_color_texture));
            material.setNormalTexture(texture_ptr(props.normal_texture));
            material.setMetallicRoughnessTexture(texture_ptr(props.metallic_roughness_texture));
            material.setEmissiveTexture(texture_ptr(props.emissive_texture));
            (void)material.saveToJson(String(absolute_path.generic_string().c_str()));
        }

        ObjectID EnsureTextureImported(AssetManager* asset_manager, const FileID& texture_id, const FsPath& asset_dir) {
            if (!asset_manager || !texture_id.isValid()) {
                return {};
            }
            const String& texture_path = texture_id.getPath();
            if (texture_path.empty()) {
                return {};
            }
            const FsPath absolute_path = asset_dir / FsPath(texture_path.c_str());
            std::error_code ec;
            if (!std::filesystem::exists(absolute_path, ec) || ec) {
                return {};
            }
            return asset_manager->ensureImported(String(absolute_path.generic_string().c_str()));
        }

        void EnsureMaterialTextures(AssetManager* asset_manager, const MaterialProperties& props, const FsPath& asset_dir,
                                    UnorderedMap<String, ObjectID>& resolved_textures, DynamicArray<ObjectID>& out_dependencies) {
            const FileID* texture_ids[] = {
                &props.base_color_texture,
                &props.normal_texture,
                &props.metallic_roughness_texture,
                &props.emissive_texture,
            };

            for (const FileID* texture_id : texture_ids) {
                if (!texture_id->isValid()) {
                    continue;
                }
                const String& texture_path = texture_id->getPath();

                ObjectID imported{};
                const auto cached_it = resolved_textures.find(texture_path);
                if (cached_it != resolved_textures.end()) {
                    imported = cached_it->second;
                } else {
                    imported = EnsureTextureImported(asset_manager, *texture_id, asset_dir);
                    resolved_textures[texture_path] = imported;
                }

                if (!imported.isValid()) {
                    continue;
                }

                Bool already_tracked = false;
                for (const auto& existing : out_dependencies) {
                    if (existing == imported) {
                        already_tracked = true;
                        break;
                    }
                }
                if (!already_tracked) {
                    out_dependencies.push_back(imported);
                }
            }
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

        UnorderedMap<String, ObjectID> resolved_textures{};
        DynamicArray<ObjectID> mesh_dependencies{};

        for (Size_t i = 0; i < baked_materials.size(); ++i) {
            DynamicArray<ObjectID> material_dependencies{};
            EnsureMaterialTextures(asset_manager, baked_materials[i], asset_dir, resolved_textures, material_dependencies);

            for (const auto& dep : material_dependencies) {
                Bool already_tracked = false;
                for (const auto& existing : mesh_dependencies) {
                    if (existing == dep) {
                        already_tracked = true;
                        break;
                    }
                }
                if (!already_tracked) {
                    mesh_dependencies.push_back(dep);
                }
            }

            const String file_name = String(fmt::format("{}_{}.domat", model_stem, i).c_str());
            const String source_path = String(("materials/" + file_name).c_str());

            UUID asset_id = asset_manager->registerAsset(source_path, AssetType::Material);
            if (!asset_id.isValid()) {
                continue;
            }
            if (Asset* existing_material = asset_manager->findAsset(asset_id)) {
                auto& material_meta = existing_material->getMetaDataMutable();
                material_meta.dependencies = material_dependencies;
                if (AssetDatabase* database = asset_manager->getDatabase()) {
                    database->setMetaData(ObjectID{asset_id, 0}, material_meta);
                }
            } else {
                auto mat = create_scope<MaterialAsset>();
                mat->setObjectID(ObjectID{asset_id, 0});
                mat->setName(FileSystem::PathToNameNoExt(source_path));
                AssetMetaData meta;
                meta.ref = ObjectID{asset_id, 0};
                meta.type = AssetType::Material;
                meta.source_file = FileID(source_path);
                meta.source_path = source_path;
                meta.dependencies = material_dependencies;
                mat->setMetaData(meta);
                asset_manager->registerMaterialAsset(std::move(mat));
            }

            WriteMaterialFile(materials_dir, file_name, baked_materials[i], resolved_textures);
        }

        auto& mesh_meta = mesh->getMetaDataMutable();
        mesh_meta.dependencies = mesh_dependencies;

        return mesh;
    }

} // dodoe
