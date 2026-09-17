// do@Redlive

#pragma once

#include <QWidget>

#include "bridge/EditorBackend.h"
#include "core/Signal.h"

#include <filesystem>
#include <vector>

#include <QHash>
#include <QPixmap>
#include <QStringList>

class QPoint;
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QComboBox;
class QFileSystemWatcher;
class AssetTreeWidget;
class QLabel;
class QScrollArea;
class QListWidget;
class QListWidgetItem;

namespace cakery {

class EditorWorkspaceContext;

class ProjectPanel : public QWidget {
    Q_OBJECT
public:
    explicit ProjectPanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);

    void refresh();

signals:
    void assetSelected(const cakery::AssetBrowserEntry& asset);
    void assetSelectionCleared();

private:
    void reloadAssets();
    void addDirectory(QTreeWidgetItem* parentItem, const std::filesystem::path& directory);
    bool filterTreeItem(QTreeWidgetItem* item, const QString& filter);
    std::filesystem::path selectedDirectory() const;
    QStringList selectedTreePaths() const;
    QStringList selectedGridPaths() const;
    QStringList findAssetReferenceHolders(std::uint64_t guid) const;
    void onContextMenu(const QPoint& pos);
    void onGridContextMenu(const QPoint& pos);
    void openAssetMenu(const QStringList& paths, const QPoint& globalPos);
    void onNewScene();
    void onNewFolder();
    void onImportAsset();
    void handleAssetDrop(const QStringList& paths, const QStringList& externalFiles,
                         const QString& targetDir);
    void moveAssetsTo(const QStringList& paths, const std::filesystem::path& targetDir);
    void importExternalFiles(const QStringList& files, const std::filesystem::path& targetDir);
    void reimportAssets(const QStringList& paths);
    void duplicateAssets(const QStringList& paths);
    void renameAsset(const QString& path);
    void deleteAssets(const QStringList& paths);
    void revealAssets(const QStringList& paths);
    void onDocumentDoubleClicked(QTreeWidgetItem* item, int column);
    void updatePreview(QTreeWidgetItem* item);
    void populateAssetGrid(const std::filesystem::path& directory);
    void selectTreeAsset(const QString& path);
    QPixmap loadThumbnail(const AssetBrowserEntry& asset);

    EditorWorkspaceContext& m_context;
    QTreeWidget* m_tree = nullptr;
    QLabel* m_preview = nullptr;
    QScrollArea* m_previewScroll = nullptr;
    QListWidget* m_assetGrid = nullptr;
    QLineEdit* m_filter = nullptr;
    QComboBox* m_typeFilter = nullptr;
    QFileSystemWatcher* m_watcher = nullptr;
    bool m_refreshPending = false;
    ScopedConnection m_assetDbSubscription;
    std::filesystem::path m_root;
    std::filesystem::path m_gridDirectory;
    std::vector<AssetBrowserEntry> m_assets;
    QHash<QString, QPair<qint64, QPixmap>> m_thumbnailCache;
};

} // namespace cakery
