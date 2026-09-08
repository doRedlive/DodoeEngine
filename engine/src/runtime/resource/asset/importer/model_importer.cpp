// do@Redlive

#include "model_importer.h"

#include "runtime/resource/file/file_system.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/asset/asset_database.h"
#include "runtime/resource/asset/types/mesh_asset.h"
#include "runtime/resource/parser/mesh_blob.h"
#include "runtime/function/render/material/material.h"
#include "runtime/resource/resource_manager.h"

#include <filesystem>

namespace dodoe {

    namespace {

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

        AssetManager* asset_manager = ResourceManager::Self().getAssetManager();
        if (!asset_manager) {
            return mesh;
        }

        const FsPath asset_dir = asset_manager->getAssetDir();
        const FsPath materials_dir = asset_dir / "materials";
        const String model_stem = FileSystem::PathToNameNoExt(ctx.source_path);

        MeshBlob blob{};
        DynamicArray<MaterialProperties> baked_materials{};
        if (!BuildMeshImport(ctx.absolute_source_path, asset_dir, blob, baked_materials)) {
            DO_ERROR("ModelImporter: failed to import '{}'", ctx.absolute_source_path);
            return mesh;
        }

        const String cache_path = String((ctx.absolute_source_path + ".domesh").c_str());
        if (!SaveMeshCache(cache_path, blob)) {
            DO_ERROR("ModelImporter: failed to write mesh cache '{}'", cache_path);
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

            const String source_path = MakeMaterialAssetPath(model_stem, static_cast<UInt32>(i));
            const String file_name = String(FsPath(source_path.c_str()).filename().string().c_str());

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
