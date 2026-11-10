// do@Redlive
#pragma once
#include "dopch.h"

#include "core/application/AssetController.h"
#include "core/bridge/EditorTypes.h"
#include "presentation/ViewModel.h"

namespace cakery {

struct ProjectTreeNode {
    String path;
    String name;
    bool isDirectory = false;
    String type;
    UInt64 uuid = 0;
    bool dirty = false;
    UInt32 dependencyCount = 0;
    DynamicArray<ProjectTreeNode> children;
};

struct ProjectGridEntry {
    String path;
    String name;
    bool isDirectory = false;
    String type;
    UInt64 uuid = 0;
    bool dirty = false;
};

enum class ProjectOperationResult {
    Ok,
    SourceMissing,
    DestinationExists,
    CopyFailed,
    ImportRejected,
    Failed,
};

class ProjectViewModel : public ViewModel {
public:
    ProjectViewModel(AssetController& controller, DomainEventBus& events);
    ~ProjectViewModel() override;

    FsPath root() const;
    const ProjectTreeNode* tree() const;
    const DynamicArray<AssetBrowserEntry>& assets() const { return m_assets; }
    const DynamicArray<String>& assetTypes() const { return m_assetTypes; }

    DynamicArray<ProjectGridEntry> buildGrid(const FsPath& directory) const;

    void refresh();
    void rebuild();

    bool hasClipboard() const { return !m_clipboardPaths.empty(); }
    void copyAssets(const DynamicArray<String>& paths);
    void cutAssets(const DynamicArray<String>& paths);
    void pasteAssets(const FsPath& targetDir);
    void moveAssetsTo(const DynamicArray<String>& paths, const FsPath& targetDir);
    void importExternalFiles(const DynamicArray<String>& files, const FsPath& targetDir);
    DynamicArray<String> reimportAssets(const DynamicArray<String>& paths);
    void duplicateAssets(const DynamicArray<String>& paths);

    ProjectOperationResult importAsset(const String& sourcePath, const FsPath& targetDir,
                                       bool overwrite);
    ProjectOperationResult renameAsset(const String& path, const String& newBaseName,
                                       String& outPath);
    DynamicArray<String> deleteAssets(const DynamicArray<String>& paths);
    bool createFolder(const FsPath& parent, const String& name);
    bool createScene(const FsPath& parent, const String& name);
    String writeScript(const FsPath& parent, const String& fileName, const String& content);

    UInt64 guidForPath(const String& path) const;
    DynamicArray<String> referenceHolders(UInt64 guid) const;
    bool queryThumbnail(const String& path, int size, Json& out) const;
    String uniqueDestination(const FsPath& directory, const String& fileName) const;

private:
    void rebuildTree();
    void rebuildTypes();

    AssetController& m_controller;
    DomainEventBus& m_events;
    DynamicArray<AssetBrowserEntry> m_assets;
    DynamicArray<String> m_assetTypes;
    Optional<ProjectTreeNode> m_tree;
    DynamicArray<String> m_clipboardPaths;
    bool m_clipboardCut = false;
    ScopedConnection m_assetSubscription;
};

} // namespace cakery
