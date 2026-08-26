// do@Redlive

#pragma once

#include <QWidget>

#include "bridge/EditorBackend.h"
#include "core/Signal.h"

#include <filesystem>
#include <vector>

#include <QHash>
#include <QPixmap>

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
    void onContextMenu(const QPoint& pos);
    void onNewScene();
    void onNewFolder();
    void onImportAsset();
    void onReimportAsset();
    void onRenameAsset();
    void onDeleteAsset();
    void onRevealAsset();
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
    std::vector<AssetBrowserEntry> m_assets;
    QHash<QString, QPair<qint64, QPixmap>> m_thumbnailCache;
};

} // namespace cakery
