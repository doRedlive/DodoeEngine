// do@Redlive

#include "dopch.h"

#include "ProjectViewModel.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

namespace cakery {

namespace {

bool IsInternalDirectory(const FsPath& path)
{
    const String name = path.filename().string();
    if (name == "Library" || name == "Configs") {
        return true;
    }
    return !name.empty() && name.front() == '.';
}

String LowerExtension(const FsPath& path)
{
    String extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return extension;
}

bool HasKnownAssetExtension(const FsPath& path)
{
    static const UnorderedSet<String> kExtensions = {
        ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tga", ".psd", ".hdr",
        ".obj", ".fbx", ".gltf", ".glb", ".tmj", ".tsx", ".doscn", ".domat",
        ".doaniclip", ".doanim", ".doinput", ".shader", ".cs", ".prefab",
        ".wav", ".ogg", ".mp3", ".flac",
    };
    return kExtensions.find(LowerExtension(path)) != kExtensions.end();
}

ProjectTreeNode BuildDirectoryNode(
    const FsPath& directory,
    const UnorderedMap<String, const AssetBrowserEntry*>& assetsByPath)
{
    ProjectTreeNode node;
    node.path = directory.string();
    node.name = directory.filename().string();
    if (node.name.empty()) {
        node.name = directory.string();
    }
    node.isDirectory = true;

    std::error_code ec;
    DynamicArray<FsPath> dirs;
    DynamicArray<FsPath> files;
    for (auto it = std::filesystem::directory_iterator(directory, ec);
         it != std::filesystem::directory_iterator(); it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (it->is_directory(ec)) {
            dirs.push_back(it->path());
        } else if (it->is_regular_file(ec)) {
            files.push_back(it->path());
        }
    }
    std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());

    for (const FsPath& dir : dirs) {
        if (IsInternalDirectory(dir)) {
            continue;
        }
        node.children.push_back(BuildDirectoryNode(dir, assetsByPath));
    }

    for (const FsPath& file : files) {
        if (file.extension() == ".meta") {
            continue;
        }
        const auto it = assetsByPath.find(AssetController::normalizePath(file));
        const AssetBrowserEntry* asset = it != assetsByPath.end() ? it->second : nullptr;
        if (!asset && !HasKnownAssetExtension(file)) {
            continue;
        }
        ProjectTreeNode child;
        child.path = file.string();
        child.name = file.filename().string();
        child.isDirectory = false;
        if (asset) {
            child.type = asset->type;
            child.uuid = asset->uuid;
            child.dirty = asset->dirty;
            child.dependencyCount = static_cast<UInt32>(asset->dependencies.size());
        }
        node.children.push_back(std::move(child));
    }

    return node;
}

UnorderedMap<String, const AssetBrowserEntry*> BuildAssetIndex(
    const DynamicArray<AssetBrowserEntry>& assets)
{
    UnorderedMap<String, const AssetBrowserEntry*> index;
    index.reserve(assets.size());
    for (const AssetBrowserEntry& asset : assets) {
        index[AssetController::normalizePath(FsPath(asset.path))] = &asset;
    }
    return index;
}

} // namespace

ProjectViewModel::ProjectViewModel(AssetController& controller, DomainEventBus& events)
    : m_controller(controller), m_events(events)
{
    m_assetSubscription = m_events.subscribe<AssetCatalogChangedEvent>(
        [this](const AssetCatalogChangedEvent&) { rebuild(); });
}

ProjectViewModel::~ProjectViewModel() = default;

FsPath ProjectViewModel::root() const
{
    return m_controller.projectRoot();
}

const ProjectTreeNode* ProjectViewModel::tree() const
{
    return m_tree ? &*m_tree : nullptr;
}

void ProjectViewModel::rebuild()
{
    m_assets.clear();
    m_controller.listAssets(m_assets);
    rebuildTypes();
    rebuildTree();
    notifyChanged();
}

void ProjectViewModel::refresh()
{
    m_controller.refresh();
    rebuild();
}

void ProjectViewModel::rebuildTypes()
{
    m_assetTypes.clear();
    for (const AssetBrowserEntry& asset : m_assets) {
        if (std::find(m_assetTypes.begin(), m_assetTypes.end(), asset.type) ==
            m_assetTypes.end()) {
            m_assetTypes.push_back(asset.type);
        }
    }
    std::sort(m_assetTypes.begin(), m_assetTypes.end());
}

void ProjectViewModel::rebuildTree()
{
    m_tree.reset();
    const FsPath projectRoot = root();
    if (projectRoot.empty()) {
        return;
    }
    const auto index = BuildAssetIndex(m_assets);
    m_tree = BuildDirectoryNode(projectRoot, index);
}

DynamicArray<ProjectGridEntry> ProjectViewModel::buildGrid(const FsPath& directory) const
{
    DynamicArray<ProjectGridEntry> entries;
    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator(directory, ec);
         it != std::filesystem::directory_iterator(); it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        const FsPath path = it->path();
        if (it->is_directory(ec)) {
            if (IsInternalDirectory(path)) {
                continue;
            }
            ProjectGridEntry entry;
            entry.path = path.string();
            entry.name = path.filename().string();
            entry.isDirectory = true;
            entries.push_back(std::move(entry));
            continue;
        }
        if (path.extension() == ".meta") {
            continue;
        }
        const AssetBrowserEntry* asset = nullptr;
        const String normalized = AssetController::normalizePath(path);
        for (const AssetBrowserEntry& candidate : m_assets) {
            if (AssetController::normalizePath(FsPath(candidate.path)) == normalized) {
                asset = &candidate;
                break;
            }
        }
        if (!asset && !HasKnownAssetExtension(path)) {
            continue;
        }
        ProjectGridEntry entry;
        entry.path = path.string();
        entry.name = path.filename().string();
        entry.isDirectory = false;
        if (asset) {
            entry.type = asset->type;
            entry.uuid = asset->uuid;
            entry.dirty = asset->dirty;
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

void ProjectViewModel::copyAssets(const DynamicArray<String>& paths)
{
    if (paths.empty()) {
        return;
    }
    m_clipboardPaths = paths;
    m_clipboardCut = false;
}

void ProjectViewModel::cutAssets(const DynamicArray<String>& paths)
{
    if (paths.empty()) {
        return;
    }
    m_clipboardPaths = paths;
    m_clipboardCut = true;
}

void ProjectViewModel::pasteAssets(const FsPath& targetDir)
{
    if (m_clipboardPaths.empty()) {
        return;
    }
    bool changed = false;
    for (const String& path : m_clipboardPaths) {
        const FsPath source(path.c_str());
        std::error_code ec;
        if (!std::filesystem::is_regular_file(source, ec)) {
            continue;
        }
        const String destination =
            m_controller.uniqueDestination(targetDir, source.filename().string());
        if (AssetController::normalizePath(source) ==
            AssetController::normalizePath(FsPath(destination.c_str()))) {
            continue;
        }
        const bool moved = m_clipboardCut
            ? m_controller.moveFile(path, FsPath(destination.c_str()))
            : m_controller.copyFile(path, FsPath(destination.c_str()));
        if (!moved) {
            continue;
        }
        m_controller.importAsset(destination);
        changed = true;
    }
    if (m_clipboardCut) {
        m_clipboardPaths.clear();
        m_clipboardCut = false;
    }
    if (changed) {
        refresh();
    }
}

void ProjectViewModel::moveAssetsTo(const DynamicArray<String>& paths, const FsPath& targetDir)
{
    bool changed = false;
    const String targetNormalized = AssetController::normalizePath(targetDir);
    for (const String& path : paths) {
        const FsPath source(path.c_str());
        std::error_code ec;
        if (!std::filesystem::is_regular_file(source, ec)) {
            continue;
        }
        if (AssetController::normalizePath(source.parent_path()) == targetNormalized) {
            continue;
        }
        const FsPath destination = targetDir / source.filename();
        if (!m_controller.moveFile(path, destination)) {
            continue;
        }
        m_controller.importAsset(destination.string());
        changed = true;
    }
    if (changed) {
        refresh();
    }
}

void ProjectViewModel::importExternalFiles(const DynamicArray<String>& files,
                                           const FsPath& targetDir)
{
    bool changed = false;
    for (const String& file : files) {
        const FsPath source(file.c_str());
        std::error_code ec;
        if (!std::filesystem::is_regular_file(source, ec)) {
            continue;
        }
        const FsPath destination = targetDir / source.filename();
        if (AssetController::normalizePath(source) ==
            AssetController::normalizePath(destination)) {
            m_controller.importAsset(destination.string());
            changed = true;
            continue;
        }
        if (std::filesystem::exists(destination, ec)) {
            continue;
        }
        if (!m_controller.copyExternalFile(file, destination, false)) {
            continue;
        }
        m_controller.importAsset(destination.string());
        changed = true;
    }
    if (changed) {
        refresh();
    }
}

DynamicArray<String> ProjectViewModel::reimportAssets(const DynamicArray<String>& paths)
{
    DynamicArray<String> failed;
    bool changed = false;
    for (const String& path : paths) {
        if (m_controller.reimportAsset(path)) {
            changed = true;
        } else {
            failed.push_back(path);
        }
    }
    if (changed) {
        refresh();
    }
    return failed;
}

void ProjectViewModel::duplicateAssets(const DynamicArray<String>& paths)
{
    bool changed = false;
    for (const String& path : paths) {
        const FsPath source(path.c_str());
        std::error_code ec;
        if (!std::filesystem::is_regular_file(source, ec)) {
            continue;
        }
        const String destination =
            m_controller.uniqueDestination(source.parent_path(), source.filename().string());
        if (!m_controller.copyFile(path, FsPath(destination.c_str()))) {
            continue;
        }
        m_controller.importAsset(destination);
        changed = true;
    }
    if (changed) {
        refresh();
    }
}

ProjectOperationResult ProjectViewModel::importAsset(const String& sourcePath,
                                                    const FsPath& targetDir,
                                                    bool overwrite)
{
    const FsPath source(sourcePath.c_str());
    std::error_code ec;
    if (!std::filesystem::is_regular_file(source, ec)) {
        return ProjectOperationResult::SourceMissing;
    }
    const FsPath destination = targetDir / source.filename();
    if (AssetController::normalizePath(source) ==
        AssetController::normalizePath(destination)) {
        if (!m_controller.importAsset(destination.string())) {
            return ProjectOperationResult::ImportRejected;
        }
        refresh();
        return ProjectOperationResult::Ok;
    }
    if (!overwrite && std::filesystem::exists(destination, ec)) {
        return ProjectOperationResult::DestinationExists;
    }
    if (!m_controller.copyExternalFile(sourcePath, destination, overwrite)) {
        return ProjectOperationResult::CopyFailed;
    }
    if (!m_controller.importAsset(destination.string())) {
        refresh();
        return ProjectOperationResult::ImportRejected;
    }
    refresh();
    return ProjectOperationResult::Ok;
}

ProjectOperationResult ProjectViewModel::renameAsset(const String& path,
                                                     const String& newBaseName,
                                                     String& outPath)
{
    if (path.empty() || newBaseName.empty() || newBaseName == "." || newBaseName == ".."
        || newBaseName.find_first_of("/\\:*?\"<>|") != String::npos
        || newBaseName.back() == '.' || newBaseName.back() == ' ') {
        return ProjectOperationResult::Failed;
    }
    const FsPath source(path.c_str());
    const String normalizedRoot = AssetController::normalizePath(root());
    const String normalizedSource = AssetController::normalizePath(source);
    if (normalizedRoot.empty() || normalizedSource == normalizedRoot
        || normalizedSource.rfind(normalizedRoot + "/", 0) != 0) {
        return ProjectOperationResult::Failed;
    }
    std::error_code ec;
    const bool directory = std::filesystem::is_directory(source, ec);
    const FsPath destination =
        source.parent_path() / (newBaseName + (directory ? String() : source.extension().string()));
    if (AssetController::normalizePath(source) ==
        AssetController::normalizePath(destination)) {
        outPath = path;
        return ProjectOperationResult::Ok;
    }
    if (!m_controller.moveFile(path, destination)) {
        return ProjectOperationResult::Failed;
    }
    if (!directory && !m_controller.importAsset(destination.string())) {
        m_controller.moveFile(destination.string(), source);
        return ProjectOperationResult::ImportRejected;
    }
    outPath = destination.string();
    refresh();
    return ProjectOperationResult::Ok;
}

DynamicArray<String> ProjectViewModel::deleteAssets(const DynamicArray<String>& paths)
{
    DynamicArray<String> failed;
    for (const String& path : paths) {
        if (!m_controller.removeFile(path)) {
            failed.push_back(path);
        }
    }
    refresh();
    return failed;
}

bool ProjectViewModel::createFolder(const FsPath& parent, const String& name)
{
    if (name.empty()) {
        return false;
    }
    if (!m_controller.createDirectory(parent / name)) {
        return false;
    }
    rebuild();
    return true;
}

bool ProjectViewModel::createScene(const FsPath& parent, const String& name)
{
    if (!m_controller.createScene(parent, name)) {
        return false;
    }
    refresh();
    return true;
}

String ProjectViewModel::writeScript(const FsPath& parent, const String& fileName,
                                     const String& content)
{
    const String destination = m_controller.uniqueDestination(parent, fileName);
    if (!m_controller.writeTextFile(FsPath(destination.c_str()), content)) {
        return {};
    }
    m_controller.importAsset(destination);
    refresh();
    return destination;
}

UInt64 ProjectViewModel::guidForPath(const String& path) const
{
    return m_controller.guidForPath(path);
}

DynamicArray<String> ProjectViewModel::referenceHolders(UInt64 guid) const
{
    return m_controller.referenceHolders(guid);
}

bool ProjectViewModel::queryThumbnail(const String& path, int size, Json& out) const
{
    return m_controller.queryThumbnail(path, size, out);
}

String ProjectViewModel::uniqueDestination(const FsPath& directory, const String& fileName) const
{
    return m_controller.uniqueDestination(directory, fileName);
}

} // namespace cakery
