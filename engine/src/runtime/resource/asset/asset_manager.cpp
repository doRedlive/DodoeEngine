// do@Redlive

#include "asset_manager.h"

#include "runtime/core/project/project.h"
#include "runtime/core/utils/common.h"
#include "runtime/resource/file/file_system.h"

namespace dodoe {

    namespace {
        const std::vector<std::string> k_ImageExts = {
            ".png",
            ".jpg",
            ".jpeg",
            ".bmp",
            ".gif",
            ".tga",
            ".psd",
            ".hdr"
        };

        const std::vector<std::string> k_ModelExts = {
            ".obj",
            ".fbx"
        };
    }

    Scope<AssetManager> AssetManager::Create() {
        if (auto manager = create_scope<AssetManager>(); manager->initialize())
            return manager;
        return nullptr;
    }

    void AssetManager::Destroy(Scope<AssetManager>& manager) {
        if (!manager) return;
        manager->shutdown();
        manager.reset();
    }

    bool AssetManager::initialize() {
        if (!loadAssets()) return false;
        return true;
    }

    void AssetManager::shutdown() {
        m_asset_umap.clear();
    }

    bool AssetManager::loadAssets() {
        m_asset_umap.clear();

        std::vector<std::string> image_paths, model_paths;
        std::filesystem::path asset_dir = "tests/Projects/OnlyOne/Assets";
        if (const auto active_project = Project::ActiveProject()) {
            asset_dir = Project::ProjectDirectory() / active_project->config().asset_directory;
        }
        if (!std::filesystem::exists(asset_dir)) {
            asset_dir = "tests/Projects/OnlyOne/Assets";
        }

        if (!std::filesystem::is_directory(asset_dir)) {
            return false;
        }
        try {
            for (const fs::directory_entry& entry : fs::recursive_directory_iterator(asset_dir)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                std::string ext = entry.path().extension().string();

                if (std::ranges::find(k_ImageExts, ext) != k_ImageExts.end()) {
                    fs::path relative_path = fs::relative(entry.path(), FileSystem::asset_path);
                    image_paths.emplace_back(relative_path.lexically_normal().generic_string());
                }
                if (std::ranges::find(k_ModelExts, ext) != k_ModelExts.end()) {
                    fs::path relative_path = fs::relative(entry.path(), FileSystem::asset_path);
                    model_paths.emplace_back(relative_path.lexically_normal().generic_string());
                }
            }
        }
        catch (const fs::filesystem_error& err) {
            DO_ERROR("Traverse {} error occur : {}", asset_dir.string(), err.what());
            return false;
        }

        for (const auto& image_path : image_paths) {
            const std::filesystem::path absolute_path = (FileSystem::asset_path / std::filesystem::path(image_path)).lexically_normal();
            const std::string normalized_path = absolute_path.generic_string();
            AssetRef ref{
                AssetHandle(static_cast<uint64_t>(string2hash(normalized_path))),
                AssetType::Texture,
                normalized_path,
                string2hash(normalized_path)
            };
            m_asset_umap[ref.handle] = ref;
        }
        for (const auto& model_path : model_paths) {
            const std::filesystem::path absolute_path = (FileSystem::asset_path / std::filesystem::path(model_path)).lexically_normal();
            const std::string normalized_path = absolute_path.generic_string();
            AssetRef ref{
                AssetHandle(static_cast<uint64_t>(string2hash(normalized_path))),
                AssetType::Model,
                normalized_path,
                string2hash(normalized_path)
            };
            m_asset_umap[ref.handle] = ref;
        }
        return true;
    }

    std::vector<AssetRef> AssetManager::getAssetsByType(const AssetType type) const {
        std::vector<AssetRef> assets;
        assets.reserve(m_asset_umap.size());
        for (const auto &ref: m_asset_umap | std::views::values) {
            if (ref.type == type) {
                assets.push_back(ref);
            }
        }
        std::ranges::sort(assets, [](const AssetRef& a, const AssetRef& b) {
            return a.path < b.path;
        });
        return assets;
    }

} // dodoe
