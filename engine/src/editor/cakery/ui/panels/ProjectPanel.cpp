// do@Redlive

#include "ProjectPanel.h"

#include "cakery/ui/EditorWorkspaceContext.h"
#include "cakery/ui/EditorIcons.h"

#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QSaveFile>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QSplitter>
#include <QFrame>
#include <QDesktopServices>
#include <QUrl>
#include <QTreeWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QSize>
#include <QHBoxLayout>
#include <QAbstractItemView>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace cakery {

namespace {

void DrawTreeBranchIndicator(const QTreeWidget* tree, QPainter* painter, const QRect& rect,
                             const QModelIndex& index)
{
    painter->save();
    painter->fillRect(rect, tree->palette().color(QPalette::Base));
    if (!tree->model()->hasChildren(index)) {
        painter->restore();
        return;
    }

    const int centerX = rect.right() - tree->indentation() / 2;
    const int centerY = rect.center().y();
    QPainterPath path;
    if (tree->isExpanded(index)) {
        path.moveTo(centerX - 4, centerY - 2);
        path.lineTo(centerX, centerY + 2);
        path.lineTo(centerX + 4, centerY - 2);
    } else {
        path.moveTo(centerX - 2, centerY - 4);
        path.lineTo(centerX + 2, centerY);
        path.lineTo(centerX - 2, centerY + 4);
    }
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(tree->palette().color(QPalette::Text), 1.5,
                         Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->drawPath(path);
    painter->restore();
}

class AssetTreeWidget final : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

protected:
    void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override {
        DrawTreeBranchIndicator(this, painter, rect, index);
    }

    QStringList mimeTypes() const override {
        return {QStringLiteral("application/x-cakery-asset"), QStringLiteral("text/uri-list")};
    }

    QMimeData* mimeData(const QList<QTreeWidgetItem*>& items) const override {
        auto* data = new QMimeData();
        if (items.size() != 1) {
            return data;
        }
        const auto* item = items.front();
        if (item->data(0, Qt::UserRole + 1).toBool()) {
            return data;
        }
        const QString path = item->data(0, Qt::UserRole).toString();
        const QString guid = item->data(0, Qt::UserRole + 3).toString();
        if (!guid.isEmpty()) {
            data->setData("application/x-cakery-asset", (guid + "\n" + path).toUtf8());
        }
        if (!path.isEmpty()) {
            data->setUrls({QUrl::fromLocalFile(path)});
            data->setText(path);
        }
        return data;
    }
};

class AssetGridWidget final : public QListWidget {
public:
    using QListWidget::QListWidget;

protected:
    QStringList mimeTypes() const override {
        return {QStringLiteral("application/x-cakery-asset"), QStringLiteral("text/uri-list")};
    }

    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override {
        auto* data = new QMimeData();
        if (items.size() != 1) {
            return data;
        }
        const auto* item = items.front();
        const QString path = item->data(Qt::UserRole).toString();
        const QString guid = item->data(Qt::UserRole + 1).toString();
        if (!guid.isEmpty()) {
            data->setData("application/x-cakery-asset", (guid + "\n" + path).toUtf8());
        }
        if (!path.isEmpty()) {
            data->setUrls({QUrl::fromLocalFile(path)});
            data->setText(path);
        }
        return data;
    }
};

class PreviewScrollArea final : public QScrollArea {
public:
    explicit PreviewScrollArea(QWidget* parent = nullptr)
        : QScrollArea(parent)
    {
        setObjectName(QStringLiteral("projectPreviewScroll"));
        setWidgetResizable(false);
        setAlignment(Qt::AlignCenter);
        setFrameShape(QFrame::StyledPanel);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        viewport()->setCursor(Qt::OpenHandCursor);
        viewport()->installEventFilter(this);
    }

    void setWidget(QWidget* widget)
    {
        if (widget) {
            widget->installEventFilter(this);
        }
        QScrollArea::setWidget(widget);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched != viewport() && watched != widget()) {
            return QScrollArea::eventFilter(watched, event);
        }
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton || mouseEvent->button() == Qt::MiddleButton) {
                m_panning = true;
                m_lastPosition = mouseEvent->position().toPoint();
                viewport()->setCursor(Qt::ClosedHandCursor);
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_panning) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const QPoint current = mouseEvent->position().toPoint();
            const QPoint delta = current - m_lastPosition;
            m_lastPosition = current;
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (m_panning && (mouseEvent->button() == Qt::LeftButton ||
                              mouseEvent->button() == Qt::MiddleButton)) {
                m_panning = false;
                viewport()->setCursor(Qt::OpenHandCursor);
                return true;
            }
        }
        return QScrollArea::eventFilter(watched, event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
            m_panning = true;
            m_lastPosition = event->position().toPoint();
            viewport()->setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        QScrollArea::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (m_panning) {
            const QPoint current = event->position().toPoint();
            const QPoint delta = current - m_lastPosition;
            m_lastPosition = current;
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
            event->accept();
            return;
        }
        QScrollArea::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (m_panning && (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)) {
            m_panning = false;
            viewport()->setCursor(Qt::OpenHandCursor);
            event->accept();
            return;
        }
        QScrollArea::mouseReleaseEvent(event);
    }

private:
    bool m_panning = false;
    QPoint m_lastPosition;
};

std::string normalizedPath(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::path normalized = path;
    if (normalized.is_relative()) {
        normalized = std::filesystem::absolute(normalized, ec);
        ec.clear();
    }
    std::string result = normalized.lexically_normal().generic_string();
#ifdef _WIN32
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
#endif
    return result;
}

bool GetIsInternalDirectory(const std::filesystem::path& path)
{
    const std::string name = path.filename().string();
    if (name == "Library" || name == "Configs") {
        return true;
    }
    return !name.empty() && name.front() == '.';
}

bool GetHasKnownAssetExtension(const std::filesystem::path& path)
{
    const QString ext = QString::fromStdString(path.extension().string()).toLower();
    static const QStringList kExtensions = {
        QStringLiteral(".png"), QStringLiteral(".jpg"), QStringLiteral(".jpeg"),
        QStringLiteral(".bmp"), QStringLiteral(".gif"), QStringLiteral(".tga"),
        QStringLiteral(".psd"), QStringLiteral(".hdr"), QStringLiteral(".obj"),
        QStringLiteral(".fbx"), QStringLiteral(".gltf"), QStringLiteral(".glb"),
        QStringLiteral(".tmj"), QStringLiteral(".tsx"), QStringLiteral(".doscn"),
        QStringLiteral(".domat"), QStringLiteral(".doaniclip"), QStringLiteral(".doanim"),
        QStringLiteral(".doinput"), QStringLiteral(".shader"), QStringLiteral(".cs"),
        QStringLiteral(".prefab"), QStringLiteral(".wav"), QStringLiteral(".ogg"),
        QStringLiteral(".mp3"), QStringLiteral(".flac")
    };
    return kExtensions.contains(ext);
}

bool GetIsImageAsset(const AssetBrowserEntry& asset)
{
    if (asset.type == "Texture" || asset.type == "Sprite") {
        return true;
    }
    const QString ext = QString::fromStdString(asset.extension);
    static const QStringList kImageExtensions = {
        QStringLiteral(".png"), QStringLiteral(".jpg"), QStringLiteral(".jpeg"),
        QStringLiteral(".bmp"), QStringLiteral(".gif"), QStringLiteral(".tga"),
        QStringLiteral(".psd"), QStringLiteral(".hdr")
    };
    return kImageExtensions.contains(ext);
}

QString GetAssetTypeIcon(const std::string& type, const std::filesystem::path& path)
{
    if (type == "Texture" || type == "Sprite") {
        return QStringLiteral("image.svg");
    }
    if (type == "Mesh" || type == "Prefab") {
        return QStringLiteral("box.svg");
    }
    if (type == "Material") {
        return QStringLiteral("material.svg");
    }
    if (type == "Tileset") {
        return QStringLiteral("grid.svg");
    }
    if (type == "TiledMap") {
        return QStringLiteral("map.svg");
    }
    if (type == "Scene") {
        return QStringLiteral("scene.svg");
    }
    const QString ext = QString::fromStdString(path.extension().string()).toLower();
    if (ext == QStringLiteral(".png") || ext == QStringLiteral(".jpg") ||
        ext == QStringLiteral(".jpeg") || ext == QStringLiteral(".bmp") ||
        ext == QStringLiteral(".gif") || ext == QStringLiteral(".tga") ||
        ext == QStringLiteral(".psd") || ext == QStringLiteral(".hdr")) {
        return QStringLiteral("image.svg");
    }
    if (ext == QStringLiteral(".obj") || ext == QStringLiteral(".fbx") ||
        ext == QStringLiteral(".gltf") || ext == QStringLiteral(".glb") ||
        ext == QStringLiteral(".prefab")) {
        return QStringLiteral("box.svg");
    }
    if (ext == QStringLiteral(".tsx")) {
        return QStringLiteral("grid.svg");
    }
    if (ext == QStringLiteral(".tmj")) {
        return QStringLiteral("map.svg");
    }
    if (ext == QStringLiteral(".doscn")) {
        return QStringLiteral("scene.svg");
    }
    if (ext == QStringLiteral(".domat")) {
        return QStringLiteral("material.svg");
    }
    return QStringLiteral("image-minus.svg");
}

} // namespace

ProjectPanel::ProjectPanel(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent), m_context(context)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Search assets..."));
    m_filter->setClearButtonEnabled(true);
    m_typeFilter = new QComboBox(this);
    m_typeFilter->setObjectName(QStringLiteral("projectTypeFilter"));
    m_typeFilter->setMinimumContentsLength(14);
    m_typeFilter->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_typeFilter->addItem(tr("All asset types"));
    auto* filterRow = new QWidget(this);
    auto* filterLayout = new QHBoxLayout(filterRow);
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->setSpacing(6);
    filterLayout->addWidget(m_filter, 1);
    filterLayout->addWidget(m_typeFilter);
    layout->addWidget(filterRow);

    m_tree = new AssetTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setIconSize(QSize(16, 16));
    m_tree->setIndentation(16);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setDragEnabled(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tree->setFocusPolicy(Qt::StrongFocus);
    m_assetGrid = new AssetGridWidget(this);
    m_assetGrid->setObjectName(QStringLiteral("projectAssetGrid"));
    m_assetGrid->setViewMode(QListView::IconMode);
    m_assetGrid->setResizeMode(QListView::Adjust);
    m_assetGrid->setMovement(QListView::Static);
    m_assetGrid->setWrapping(true);
    m_assetGrid->setWordWrap(true);
    m_assetGrid->setIconSize(QSize(72, 72));
    m_assetGrid->setGridSize(QSize(104, 110));
    m_assetGrid->setSpacing(6);
    m_assetGrid->setDragEnabled(true);
    m_assetGrid->setSelectionMode(QAbstractItemView::SingleSelection);

    auto* contentSplitter = new QSplitter(Qt::Horizontal, this);
    contentSplitter->setObjectName(QStringLiteral("projectContentSplitter"));
    contentSplitter->setHandleWidth(1);
    contentSplitter->addWidget(m_tree);
    contentSplitter->addWidget(m_assetGrid);
    contentSplitter->setChildrenCollapsible(false);
    contentSplitter->setStretchFactor(0, 1);
    contentSplitter->setStretchFactor(1, 1);
    contentSplitter->setSizes(QList<int>{300, 240});
    layout->addWidget(contentSplitter, 1);
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString&) {
        if (m_refreshPending) return;
        m_refreshPending = true;
        QTimer::singleShot(150, this, [this]() {
            m_refreshPending = false;
            refresh();
        });
    });

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &ProjectPanel::onDocumentDoubleClicked);
    connect(m_tree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current, QTreeWidgetItem*) {
        updatePreview(current);
        if (!current || current->data(0, Qt::UserRole + 1).toBool()) {
            emit assetSelectionCleared();
            return;
        }
        const std::uint64_t guid = current->data(0, Qt::UserRole + 3).toULongLong();
        for (const auto& asset : m_assets) {
            if (asset.uuid == guid) {
                emit assetSelected(asset);
                return;
            }
        }
        emit assetSelectionCleared();
    });
    connect(m_assetGrid, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (item) {
            selectTreeAsset(item->data(Qt::UserRole).toString());
        }
    });
    connect(m_assetGrid, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (!item) {
            return;
        }
        const QString path = item->data(Qt::UserRole).toString();
        if (QFileInfo(path).suffix().compare(QLatin1String("doscn"), Qt::CaseInsensitive) == 0) {
            m_context.session().openDocument(path.toStdString());
        }
    });
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &ProjectPanel::onContextMenu);
    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (m_tree->topLevelItemCount() > 0) {
            filterTreeItem(m_tree->topLevelItem(0), text);
        }
    });
    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
        if (m_tree->topLevelItemCount() > 0) {
            filterTreeItem(m_tree->topLevelItem(0), m_filter ? m_filter->text() : QString());
        }
    });
    m_assetDbSubscription = ScopedConnection(
        m_context.session().assetDatabaseChanged,
        m_context.session().assetDatabaseChanged.connect([this]() { reloadAssets(); }));
    refresh();
}

void ProjectPanel::refresh()
{
    m_refreshPending = false;
    m_context.session().execute(EditorCommandMessage{"asset.refresh", {}});
    reloadAssets();
}

void ProjectPanel::reloadAssets()
{
    m_tree->clear();
    if (m_watcher) {
        const QStringList watched = m_watcher->directories();
        if (!watched.isEmpty()) {
            m_watcher->removePaths(watched);
        }
    }
    m_assets.clear();
    m_context.session().listAssets(m_assets);
    if (m_typeFilter) {
        const QString previous = m_typeFilter->currentData().toString();
        m_typeFilter->blockSignals(true);
        m_typeFilter->clear();
        m_typeFilter->addItem(tr("All asset types"), QString());
        std::vector<std::string> types;
        for (const auto& asset : m_assets) {
            if (std::find(types.begin(), types.end(), asset.type) == types.end()) {
                types.push_back(asset.type);
            }
        }
        std::sort(types.begin(), types.end());
        for (const auto& type : types) {
            m_typeFilter->addItem(QString::fromStdString(type), QString::fromStdString(type));
        }
        const int previousIndex = m_typeFilter->findData(previous);
        m_typeFilter->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
        m_typeFilter->blockSignals(false);
    }
    m_root = m_context.session().assetRoot();
    if (m_root.empty()) {
        auto* item = new QTreeWidgetItem(m_tree);
        item->setText(0, tr("No project open"));
        return;
    }

    if (m_watcher) {
        QStringList directories;
        std::error_code watcherEc;
        for (std::filesystem::recursive_directory_iterator it(m_root, watcherEc), end;
             it != end; it.increment(watcherEc)) {
            if (!watcherEc && it->is_directory(watcherEc)) {
                directories.push_back(QString::fromStdString(it->path().string()));
            }
            watcherEc.clear();
        }
        directories.push_back(QString::fromStdString(m_root.string()));
        m_watcher->addPaths(directories);
    }

    auto* rootItem = new QTreeWidgetItem(m_tree);
    rootItem->setText(0, QFileInfo(QString::fromStdString(m_root.string())).fileName());
    rootItem->setIcon(0, editorIcon(QStringLiteral("folder-up.svg")));
    rootItem->setData(0, Qt::UserRole, QString::fromStdString(m_root.string()));
    rootItem->setData(0, Qt::UserRole + 1, true);
    addDirectory(rootItem, m_root);
    rootItem->setExpanded(true);
    filterTreeItem(rootItem, m_filter ? m_filter->text() : QString());
    populateAssetGrid(m_root);
}

bool ProjectPanel::filterTreeItem(QTreeWidgetItem* item, const QString& filter)
{
    if (!item) {
        return false;
    }
    const QString needle = filter.trimmed();
    bool childVisible = false;
    for (int i = 0; i < item->childCount(); ++i) {
        childVisible = filterTreeItem(item->child(i), needle) || childVisible;
    }
    const bool isDirectory = item->data(0, Qt::UserRole + 1).toBool();
    const bool selfMatches = needle.isEmpty() ||
        item->text(0).contains(needle, Qt::CaseInsensitive) ||
        item->toolTip(0).contains(needle, Qt::CaseInsensitive);
    const QString selectedType = m_typeFilter ? m_typeFilter->currentData().toString() : QString();
    const QString itemType = item->data(0, Qt::UserRole + 2).toString();
    const bool typeMatches = selectedType.isEmpty() || itemType == selectedType;
    const bool visible = item->parent() == nullptr ||
        ((selfMatches && typeMatches) || (isDirectory && childVisible));
    item->setHidden(!visible);
    if (isDirectory && childVisible && !needle.isEmpty()) {
        item->setExpanded(true);
    }
    return visible;
}

void ProjectPanel::addDirectory(QTreeWidgetItem* parentItem, const std::filesystem::path& directory)
{
    std::error_code ec;
    std::vector<std::filesystem::path> dirs;
    std::vector<std::filesystem::path> files;
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

    for (const auto& dir : dirs) {
        if (GetIsInternalDirectory(dir)) {
            continue;
        }
        auto* item = new QTreeWidgetItem(parentItem);
        item->setText(0, QString::fromStdString(dir.filename().string()));
        item->setIcon(0, editorIcon(QStringLiteral("folder-up.svg")));
        item->setData(0, Qt::UserRole, QString::fromStdString(dir.string()));
        item->setData(0, Qt::UserRole + 1, true);
        addDirectory(item, dir);
    }
    for (const auto& file : files) {
        if (file.extension() == ".meta") {
            continue;
        }
        const auto normalized = normalizedPath(file);
        const AssetBrowserEntry* asset = nullptr;
        for (const auto& candidate : m_assets) {
            if (normalizedPath(std::filesystem::path(candidate.path)) == normalized) {
                asset = &candidate;
                break;
            }
        }
        if (!asset && !GetHasKnownAssetExtension(file)) {
            continue;
        }
        auto* item = new QTreeWidgetItem(parentItem);
        item->setText(0, QString::fromStdString(file.filename().string()));
        item->setIcon(0, editorIcon(GetAssetTypeIcon(asset ? asset->type : std::string(), file)));
        item->setData(0, Qt::UserRole, QString::fromStdString(file.string()));
        item->setData(0, Qt::UserRole + 1, false);
        if (asset) {
            item->setData(0, Qt::UserRole + 2, QString::fromStdString(asset->type));
            item->setData(0, Qt::UserRole + 3,
                          QString::number(static_cast<qulonglong>(asset->uuid)));
            item->setToolTip(0, QStringLiteral("%1\nGUID: %2\nType: %3%4%5")
                .arg(QString::fromStdString(asset->path))
                .arg(QString::number(static_cast<qulonglong>(asset->uuid)))
                .arg(QString::fromStdString(asset->type))
                .arg(asset->dirty ? tr("\nStatus: Import required") : QString())
                .arg(asset->dependencies.empty() ? QString() :
                     tr("\nDependencies: %1").arg(asset->dependencies.size())));
        }
    }
}

std::filesystem::path ProjectPanel::selectedDirectory() const
{
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    if (!selected.isEmpty()) {
        const QString path = selected.first()->data(0, Qt::UserRole).toString();
        if (!path.isEmpty()) {
            const QFileInfo info(path);
            if (info.isDir()) {
                return std::filesystem::path(path.toStdString());
            }
            return std::filesystem::path(info.absolutePath().toStdString());
        }
    }
    return m_root;
}

void ProjectPanel::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (item) {
        m_tree->setCurrentItem(item);
    }
    QMenu menu(this);
    QAction* newSceneAction = menu.addAction(tr("New Scene"));
    QAction* newFolderAction = menu.addAction(tr("New Folder"));
    QAction* importAssetAction = menu.addAction(tr("Import Asset..."));
    menu.addSeparator();
    QAction* reimportAssetAction = nullptr;
    QAction* renameAssetAction = nullptr;
    QAction* deleteAssetAction = nullptr;
    QAction* revealAssetAction = nullptr;
    if (item && !item->data(0, Qt::UserRole + 1).toBool()) {
        reimportAssetAction = menu.addAction(tr("Reimport Asset"));
        renameAssetAction = menu.addAction(tr("Rename"));
        deleteAssetAction = menu.addAction(tr("Delete"));
        revealAssetAction = menu.addAction(tr("Show in Explorer"));
        menu.addSeparator();
    }
    QAction* refreshAction = menu.addAction(tr("Refresh"));
    QAction* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (chosen == newSceneAction) {
        onNewScene();
    } else if (chosen == newFolderAction) {
        onNewFolder();
    } else if (chosen == importAssetAction) {
        onImportAsset();
    } else if (chosen == reimportAssetAction) {
        onReimportAsset();
    } else if (chosen == renameAssetAction) {
        onRenameAsset();
    } else if (chosen == deleteAssetAction) {
        onDeleteAsset();
    } else if (chosen == revealAssetAction) {
        onRevealAsset();
    } else if (chosen == refreshAction) {
        refresh();
    }
}

void ProjectPanel::onRenameAsset()
{
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    if (selected.isEmpty()) return;
    const QString oldPath = selected.first()->data(0, Qt::UserRole).toString();
    if (oldPath.isEmpty()) return;
    const QFileInfo oldInfo(oldPath);
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Rename Asset"), tr("Asset name:"), QLineEdit::Normal,
        oldInfo.completeBaseName(), &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    const QString suffix = oldInfo.completeSuffix().isEmpty()
        ? QString()
        : QStringLiteral(".") + oldInfo.completeSuffix();
    const QString newPath = oldInfo.absoluteDir().filePath(name + suffix);
    if (QFileInfo::exists(newPath) || !QFile::rename(oldPath, newPath)) {
        QMessageBox::warning(this, tr("Rename Asset"), tr("Could not rename the asset."));
        return;
    }
    const QString oldMeta = oldPath + QStringLiteral(".meta");
    const QString newMeta = newPath + QStringLiteral(".meta");
    if (QFileInfo::exists(oldMeta)) {
        QFile::rename(oldMeta, newMeta);
    }
    m_context.session().execute({"asset.import", newPath.toStdString()});
    refresh();
}

void ProjectPanel::onDeleteAsset()
{
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    if (selected.isEmpty()) return;
    const QString path = selected.first()->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) return;
    if (QMessageBox::question(this, tr("Delete Asset"),
                              tr("Delete the selected asset from the project?"),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    if (!QFile::remove(path)) {
        QMessageBox::warning(this, tr("Delete Asset"), tr("Could not delete the asset."));
        return;
    }
    const QString metaPath = path + QStringLiteral(".meta");
    if (QFileInfo::exists(metaPath)) {
        QFile::remove(metaPath);
    }
    refresh();
}

void ProjectPanel::onImportAsset()
{
    const QString source = QFileDialog::getOpenFileName(
        this, tr("Import Asset"), QString(),
        tr("Images and Models (*.png *.jpg *.jpeg *.bmp *.obj *.fbx *.gltf *.glb);;All Files (*)"));
    if (source.isEmpty() || m_root.empty()) {
        return;
    }

    const std::filesystem::path destinationDir = selectedDirectory();
    if (destinationDir.empty()) {
        return;
    }
    const QString destination = QString::fromStdString(
        (destinationDir / QFileInfo(source).fileName().toStdString()).string());
    if (QFileInfo(source).absoluteFilePath() == QFileInfo(destination).absoluteFilePath()) {
        if (!m_context.session().execute({"asset.import", destination.toStdString()})) {
            QMessageBox::warning(this, tr("Import Asset"), tr("The asset importer rejected this file."));
        }
        refresh();
        return;
    }
    if (QFileInfo::exists(destination)) {
        const auto choice = QMessageBox::question(
            this, tr("Import Asset"), tr("An asset with this name already exists. Replace it?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (choice != QMessageBox::Yes) {
            return;
        }
    }
    QFile sourceFile(source);
    QSaveFile destinationFile(destination);
    if (!sourceFile.open(QIODevice::ReadOnly) ||
        !destinationFile.open(QIODevice::WriteOnly) ||
        destinationFile.write(sourceFile.readAll()) < 0 ||
        !destinationFile.commit()) {
        QMessageBox::warning(this, tr("Import Asset"), tr("Could not copy the selected file."));
        return;
    }
    if (!m_context.session().execute({"asset.import", destination.toStdString()})) {
        QMessageBox::warning(
            this, tr("Import Asset"),
            tr("The file was copied into Assets, but no importer accepted its format."));
    }
    refresh();
}

void ProjectPanel::onReimportAsset()
{
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    const QString path = selected.first()->data(0, Qt::UserRole).toString();
    if (!path.isEmpty()) {
        if (!m_context.session().execute({"asset.reimport", path.toStdString()})) {
            QMessageBox::warning(this, tr("Reimport Asset"), tr("The selected asset could not be reimported."));
        }
        refresh();
    }
}

void ProjectPanel::onRevealAsset()
{
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    const QString path = selected.first()->data(0, Qt::UserRole).toString();
    if (!path.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    }
}

void ProjectPanel::updatePreview(QTreeWidgetItem* item)
{
    if (!item) {
        return;
    }
    const QString path = item->data(0, Qt::UserRole).toString();
    if (item->data(0, Qt::UserRole + 1).toBool()) {
        populateAssetGrid(std::filesystem::path(path.toStdString()));
    }
}

void ProjectPanel::populateAssetGrid(const std::filesystem::path& directory)
{
    if (!m_assetGrid) {
        return;
    }
    m_assetGrid->clear();
    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator(directory, ec);
         it != std::filesystem::directory_iterator(); it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        const auto path = it->path();
        if (it->is_directory(ec)) {
            if (GetIsInternalDirectory(path)) {
                continue;
            }
            auto* item = new QListWidgetItem(m_assetGrid);
            item->setData(Qt::UserRole, QString::fromStdString(path.string()));
            item->setText(QString::fromStdString(path.filename().string()));
            item->setIcon(editorIcon(QStringLiteral("folder-up.svg")));
            continue;
        }
        if (path.extension() == ".meta") {
            continue;
        }
        const auto normalized = normalizedPath(path);
        const AssetBrowserEntry* asset = nullptr;
        for (const auto& candidate : m_assets) {
            if (normalizedPath(std::filesystem::path(candidate.path)) == normalized) {
                asset = &candidate;
                break;
            }
        }
        if (!asset && !GetHasKnownAssetExtension(path)) {
            continue;
        }
        auto* item = new QListWidgetItem(m_assetGrid);
        item->setData(Qt::UserRole, QString::fromStdString(path.string()));
        item->setText(QString::fromStdString(path.filename().string()));
        if (asset) {
            item->setData(Qt::UserRole + 1, QString::number(static_cast<qulonglong>(asset->uuid)));
            item->setToolTip(QStringLiteral("%1\n%2").arg(QString::fromStdString(asset->type),
                                                            QString::fromStdString(asset->path)));
            const QPixmap thumbnail = loadThumbnail(*asset);
            item->setIcon(thumbnail.isNull()
                              ? editorIcon(GetAssetTypeIcon(asset->type, path))
                              : QIcon(thumbnail));
            if (asset->dirty) {
                item->setText(item->text() + QStringLiteral(" ⚠"));
            }
        } else {
            item->setIcon(editorIcon(GetAssetTypeIcon(std::string(), path)));
        }
    }
}

void ProjectPanel::selectTreeAsset(const QString& path)
{
    const auto items = m_tree->findItems(QString(), Qt::MatchContains | Qt::MatchRecursive);
    for (QTreeWidgetItem* item : items) {
        if (item->data(0, Qt::UserRole).toString() == path) {
            m_tree->setCurrentItem(item);
            return;
        }
    }
}

QPixmap ProjectPanel::loadThumbnail(const AssetBrowserEntry& asset)
{
    const QString path = QString::fromStdString(asset.path);
    const QString key = QString::fromStdString(normalizedPath(std::filesystem::path(asset.path)));
    const qint64 mtime = QFileInfo(path).lastModified().toMSecsSinceEpoch();
    const auto it = m_thumbnailCache.constFind(key);
    if (it != m_thumbnailCache.constEnd() && it->first == mtime) {
        return it->second;
    }

    QPixmap pixmap;
    if (GetIsImageAsset(asset)) {
        QImage image(path);
        if (!image.isNull()) {
            pixmap = QPixmap::fromImage(image.scaled(
                QSize(72, 72), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    } else {
        nlohmann::json out;
        if (m_context.session().queryAssetThumbnail(asset.path, 96, out) && out.is_object()) {
            const int width = out.value("width", 0);
            const int height = out.value("height", 0);
            if (width > 0 && height > 0 && out.contains("data") && out["data"].is_string()) {
                const QByteArray bytes = QByteArray::fromBase64(
                    QByteArray::fromStdString(out["data"].get<std::string>()));
                if (bytes.size() >= width * height * 4) {
                    const QImage image(reinterpret_cast<const uchar*>(bytes.constData()),
                                       width, height, width * 4, QImage::Format_RGBA8888);
                    if (!image.isNull()) {
                        pixmap = QPixmap::fromImage(image.copy().scaled(
                            QSize(72, 72), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    }
                }
            }
        }
    }

    if (!pixmap.isNull()) {
        m_thumbnailCache.insert(key, qMakePair(mtime, pixmap));
    }
    return pixmap;
}

void ProjectPanel::onNewScene()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("New Scene"), tr("Scene name:"), QLineEdit::Normal,
        tr("New Scene"), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    const std::filesystem::path dir = selectedDirectory();
    if (m_context.session().newScene(dir, name.trimmed().toStdString())) {
        refresh();
    }
}

void ProjectPanel::onNewFolder()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("New Folder"), tr("Folder name:"), QLineEdit::Normal,
        tr("New Folder"), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    std::error_code ec;
    const std::filesystem::path dir = selectedDirectory() / name.trimmed().toStdString();
    std::filesystem::create_directory(dir, ec);
    if (!ec) {
        refresh();
    }
}

void ProjectPanel::onDocumentDoubleClicked(QTreeWidgetItem* item, int column)
{
    (void)column;
    if (!item) {
        return;
    }
    if (item->data(0, Qt::UserRole + 1).toBool()) {
        return;
    }
    const QString path = item->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().compare(QLatin1String("doscn"), Qt::CaseInsensitive) == 0) {
        m_context.session().openDocument(path.toStdString());
    }
}

} // namespace cakery
