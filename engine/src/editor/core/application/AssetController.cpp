// do@Redlive

#include "dopch.h"

#include "AssetController.h"

#include "core/bridge/EditorCommands.h"
#include "core/document/EditorDocumentModel.h"
#include "core/EditorSession.h"
#include "services/AssetFileOperations.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

namespace cakery {

AssetController::AssetController(EditorSession& session, DomainEventBus& events)
    : m_session(session), m_events(events)
{
    m_missingSubscription = ScopedConnection(
        m_session.missingAssetReferencesDetected,
        m_session.missingAssetReferencesDetected.connect([this](Size_t count) {
            missingAssetReferences.fire(count);
        }));
}

AssetController::~AssetController() = default;

FsPath AssetController::projectRoot() const
{
    return m_session.assetRoot();
}

bool AssetController::listAssets(DynamicArray<AssetBrowserEntry>& out) const
{
    return m_session.listAssets(out);
}

bool AssetController::queryThumbnail(const String& path, int size, Json& out) const
{
    return m_session.queryAssetThumbnail(path, size, out);
}

bool AssetController::getImportSettings(const String& path, AssetImportSettings& out) const
{
    return m_session.getAssetImportSettings(path, out);
}

bool AssetController::updateImportSettings(const String& path, const Json& settings)
{
    return report(m_session.execute(AssetUpdateSettingsCommand{path, settings}),
                  "Update import settings");
}

UInt64 AssetController::guidForPath(const String& path) const
{
    const String normalized = normalizePath(FsPath(path.c_str()));
    DynamicArray<AssetBrowserEntry> assets;
    if (!m_session.listAssets(assets)) {
        return 0;
    }
    for (const AssetBrowserEntry& asset : assets) {
        if (normalizePath(FsPath(asset.path)) == normalized) {
            return asset.uuid;
        }
    }
    return 0;
}

DynamicArray<String> AssetController::referenceHolders(UInt64 guid) const
{
    const String needle = "\"asset_id\":" + ToString(guid);
    DynamicArray<String> holders;
    for (const EditorEntity& entity : m_session.documentModel().entities()) {
        bool references = false;
        for (const EditorComponent& component : entity.nativeComponents) {
            if (ToString(component.value).find(needle) != String::npos) {
                references = true;
                break;
            }
        }
        if (!references) {
            for (const EditorComponent& component : entity.managedComponents) {
                if (ToString(component.value).find(needle) != String::npos) {
                    references = true;
                    break;
                }
            }
        }
        if (references) {
            holders.push_back(entity.name);
        }
    }
    return holders;
}

bool AssetController::refreshPending() const
{
    return m_session.isAssetRefreshPending();
}

void AssetController::refreshProgress(Size_t& done, Size_t& total) const
{
    m_session.assetRefreshProgress(done, total);
}

void AssetController::refresh()
{
    m_session.execute(AssetRefreshCommand{});
}

bool AssetController::importAsset(const String& path)
{
    return report(m_session.execute(AssetImportCommand{path}), "Import asset");
}

bool AssetController::reimportAsset(const String& path)
{
    return report(m_session.execute(AssetReimportCommand{path}), "Reimport asset");
}

bool AssetController::createScene(const FsPath& directory, const String& name)
{
    return report(m_session.newScene(directory, name), "Create scene");
}

bool AssetController::copyFile(const String& source, const FsPath& target) const
{
    return AssetFileOperations::copyFile(FsPath(source.c_str()), target) ==
        AssetFileResult::Ok;
}

bool AssetController::moveFile(const String& source, const FsPath& target) const
{
    return AssetFileOperations::moveFile(FsPath(source.c_str()), target) ==
        AssetFileResult::Ok;
}

bool AssetController::removeFile(const String& path) const
{
    return AssetFileOperations::removeWithMeta(FsPath(path.c_str()));
}

bool AssetController::createDirectory(const FsPath& directory) const
{
    std::error_code ec;
    std::filesystem::create_directory(directory, ec);
    return !ec;
}

bool AssetController::writeTextFile(const FsPath& path, const String& content) const
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return false;
    }
    stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(stream);
}

bool AssetController::copyExternalFile(const String& source, const FsPath& target,
                                       bool overwrite) const
{
    const FsPath sourcePath(source.c_str());
    std::error_code ec;
    if (!std::filesystem::is_regular_file(sourcePath, ec)) {
        return false;
    }
    if (std::filesystem::equivalent(sourcePath, target, ec) && !ec) {
        return true;
    }
    ec.clear();
    if (std::filesystem::exists(target, ec) && !overwrite) {
        return false;
    }
    ec.clear();
    const auto options = overwrite
        ? std::filesystem::copy_options::overwrite_existing
        : std::filesystem::copy_options::none;
    std::filesystem::copy_file(sourcePath, target, options, ec);
    return !ec;
}

String AssetController::uniqueDestination(const FsPath& directory, const String& fileName) const
{
    const FsPath name(fileName.c_str());
    const String base = name.stem().string();
    const String suffix = name.extension().string();
    FsPath candidate = directory / name;
    for (int index = 1; std::filesystem::exists(candidate) && index < 1000; ++index) {
        candidate = directory / (base + " " + std::to_string(index) + suffix);
    }
    return candidate.string();
}

String AssetController::normalizePath(const FsPath& path)
{
    std::error_code ec;
    FsPath normalized = path;
    if (normalized.is_relative()) {
        normalized = std::filesystem::absolute(normalized, ec);
        ec.clear();
    }
    String result(normalized.lexically_normal().generic_string().c_str());
#ifdef _WIN32
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
#endif
    return result;
}

ScopedConnection AssetController::subscribeChanged(std::function<void()> onChange)
{
    return ScopedConnection(m_session.assetDatabaseChanged,
                            m_session.assetDatabaseChanged.connect(std::move(onChange)));
}

bool AssetController::report(bool ok, const char* action)
{
    if (ok) {
        return true;
    }
    NotificationEvent event;
    event.level = "error";
    event.message = action;
    event.source = m_session.diagnostic();
    m_events.publish(event);
    return false;
}

} // namespace cakery
