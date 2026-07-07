#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QToolButton>
#include <QLabel>
#include <QSlider>
#include <QSplitter>
#include <QMimeData>

namespace cakery {

class AssetListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit AssetListWidget(QWidget* parent = nullptr) : QListWidget(parent) {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
    }

protected:
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override {
        if (items.isEmpty()) return nullptr;
        auto* mime = new QMimeData();
        QStringList paths;
        for (auto* item : items) {
            paths << item->data(Qt::UserRole).toString();
        }
        mime->setText(paths.join('\n'));
        mime->setData("application/x-cakery-asset", paths.join('\n').toUtf8());
        return mime;
    }

    QStringList mimeTypes() const override {
        return {"application/x-cakery-asset", "text/uri-list", "text/plain"};
    }
};

class ProjectBrowserWidget : public QWidget {
    Q_OBJECT
public:
    explicit ProjectBrowserWidget(QWidget* parent = nullptr);

    void setBasePath(const QString& path);
    void refresh();

private slots:
    void onFolderSelected();
    void onAssetDoubleClicked(QListWidgetItem* item);
    void onIconSizeChanged(int value);
    void onCreateFolder();
    void onRefresh();
    void onSearchChanged(const QString& text);
    void onTreeContextMenu(const QPoint& pos);
    void onAssetContextMenu(const QPoint& pos);
    void onRenameFolder();
    void onDeleteFolder();
    void onShowInExplorer(const QString& path);

private:
    void buildFolderTree(const QString& rootPath);
    void populateAssets(const QString& folderPath);
    QIcon iconForFile(const QString& fileName) const;
    QIcon fileTypeIcon(const QString& suffix) const;

    QToolButton* m_createBtn = nullptr;
    QToolButton* m_refreshBtn = nullptr;
    QLineEdit* m_searchEdit = nullptr;

    QSplitter* m_splitter = nullptr;
    QTreeWidget* m_folderTree = nullptr;
    QLabel* m_breadcrumb = nullptr;
    AssetListWidget* m_assetList = nullptr;

    QLabel* m_pathLabel = nullptr;
    QSlider* m_iconSizeSlider = nullptr;

    QString m_basePath;
    QString m_currentFolder;
};

}
