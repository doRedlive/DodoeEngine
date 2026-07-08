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
#include <QDrag>
#include <QMouseEvent>
#include <QApplication>

namespace cakery {

class AssetListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit AssetListWidget(QWidget* parent = nullptr) : QListWidget(parent) {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setDefaultDropAction(Qt::CopyAction);
        setSelectionRectVisible(false);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        m_dragStartPos = event->pos();
        m_dragStarted = false;

        QListWidgetItem* item = itemAt(event->pos());
        if (item && !item->isSelected()) {
            setCurrentItem(item);
        }

        QListWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (m_dragStarted) return;

        if (event->buttons() & Qt::LeftButton) {
            int dx = event->pos().x() - m_dragStartPos.x();
            int dy = event->pos().y() - m_dragStartPos.y();
            int threshold = QApplication::startDragDistance();

            if (dx * dx + dy * dy >= threshold * threshold) {
                QListWidgetItem* item = itemAt(m_dragStartPos);
                if (item) {
                    m_dragStarted = true;
                    setCurrentItem(item);
                    startDrag(Qt::CopyAction);
                    return;
                }
            }
        }

        QListWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        m_dragStarted = false;
        QListWidget::mouseReleaseEvent(event);
    }

    void startDrag(Qt::DropActions supportedActions) override {
        QList<QListWidgetItem*> items = selectedItems();
        if (items.isEmpty()) return;

        QMimeData* data = mimeData(items);
        if (!data) return;

        QDrag* drag = new QDrag(this);
        drag->setMimeData(data);

        QPixmap pixmap = items.first()->icon().pixmap(48, 48);
        if (!pixmap.isNull()) {
            drag->setPixmap(pixmap);
            drag->setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));
        }

        drag->exec(supportedActions, Qt::CopyAction);
    }

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

private:
    QPoint m_dragStartPos;
    bool m_dragStarted = false;
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
