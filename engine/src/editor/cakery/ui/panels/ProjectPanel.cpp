// do@Redlive

#include "ProjectPanel.h"

#include "cakery/ui/EditorWorkspaceContext.h"
#include "cakery/ui/EditorIcons.h"
#include "core/document/EditorDocumentModel.h"

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
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDebug>
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
#include <QShortcut>
#include <QKeySequence>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
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

    std::function<void(const QStringList& paths, const QStringList& externalFiles,
                       const QString& targetDir)> dropHandler;

protected:
    void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override {
        DrawTreeBranchIndicator(this, painter, rect, index);
    }

    QStringList mimeTypes() const override {
        return {QStringLiteral("application/x-cakery-asset"), QStringLiteral("text/uri-list")};
    }

    QMimeData* mimeData(const QList<QTreeWidgetItem*>& items) const override {
        auto* data = new QMimeData();
        QStringList entries;
        QList<QUrl> urls;
        for (const QTreeWidgetItem* item : items) {
            if (item->data(0, Qt::UserRole + 1).toBool()) {
                continue;
            }
            const QString path = item->data(0, Qt::UserRole).toString();
            const QString guid = item->data(0, Qt::UserRole + 3).toString();
            if (path.isEmpty()) {
                continue;
            }
            urls.append(QUrl::fromLocalFile(path));
            if (!guid.isEmpty()) {
                entries << guid << path;
            }
        }
        if (!entries.isEmpty()) {
            data->setData("application/x-cakery-asset", entries.join('\n').toUtf8());
        }
        if (!urls.isEmpty()) {
            data->setUrls(urls);
            data->setText(urls.first().toLocalFile());
        }
        return data;
    }

    void dragEnterEvent(QDragEnterEvent* event) override {
        if (IsAssetDrag(event->mimeData())) {
            event->acceptProposedAction();
            return;
        }
        QTreeWidget::dragEnterEvent(event);
    }

    void dragMoveEvent(QDragMoveEvent* event) override {
        if (IsAssetDrag(event->mimeData())) {
            event->acceptProposedAction();
            return;
        }
        QTreeWidget::dragMoveEvent(event);
    }

    void dropEvent(QDropEvent* event) override {
        const QMimeData* mime = event->mimeData();
        if (!IsAssetDrag(mime)) {
            QTreeWidget::dropEvent(event);
            return;
        }
        QString targetDir;
        if (QTreeWidgetItem* item = itemAt(event->position().toPoint())) {
            const QString path = item->data(0, Qt::UserRole).toString();
            if (item->data(0, Qt::UserRole + 1).toBool()) {
                targetDir = path;
            } else if (!path.isEmpty()) {
                targetDir = QFileInfo(path).absolutePath();
            }
        }
        if (targetDir.isEmpty() && topLevelItemCount() > 0) {
            targetDir = topLevelItem(0)->data(0, Qt::UserRole).toString();
        }
        QStringList paths;
        QStringList externalFiles;
        if (mime->hasFormat(QStringLiteral("application/x-cakery-asset"))) {
            const QList<QByteArray> parts = mime->data(
                QStringLiteral("application/x-cakery-asset")).split('\n');
            for (int i = 1; i < parts.size(); i += 2) {
                const QString path = QString::fromUtf8(parts[i]);
                if (!path.isEmpty()) {
                    paths.append(path);
                }
            }
        } else {
            for (const QUrl& url : mime->urls()) {
                if (url.isLocalFile()) {
                    externalFiles.append(url.toLocalFile());
                }
            }
        }
        if (!targetDir.isEmpty() && dropHandler) {
            dropHandler(paths, externalFiles, targetDir);
        }
        event->acceptProposedAction();
    }

private:
    static bool IsAssetDrag(const QMimeData* mime) {
        return mime->hasFormat(QStringLiteral("application/x-cakery-asset")) || mime->hasUrls();
    }
};

class AssetGridWidget final : public QListWidget {
public:
    using QListWidget::QListWidget;

    std::function<void(const QStringList& paths, const QStringList& externalFiles,
                       const QString& targetDir)> dropHandler;

protected:
    QStringList mimeTypes() const override {
        return {QStringLiteral("application/x-cakery-asset"), QStringLiteral("text/uri-list")};
    }

    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override {
        auto* data = new QMimeData();
        QStringList entries;
        QList<QUrl> urls;
        for (const QListWidgetItem* item : items) {
            const QString path = item->data(Qt::UserRole).toString();
            const QString guid = item->data(Qt::UserRole + 1).toString();
            if (path.isEmpty() || QFileInfo(path).isDir()) {
                continue;
            }
            urls.append(QUrl::fromLocalFile(path));
            if (!guid.isEmpty()) {
                entries << guid << path;
            }
        }
        if (!entries.isEmpty()) {
            data->setData("application/x-cakery-asset", entries.join('\n').toUtf8());
        }
        if (!urls.isEmpty()) {
            data->setUrls(urls);
            data->setText(urls.first().toLocalFile());
        }
        return data;
    }

    void dragEnterEvent(QDragEnterEvent* event) override {
        if (IsAssetDrag(event->mimeData())) {
            event->acceptProposedAction();
            return;
        }
        QListWidget::dragEnterEvent(event);
    }

    void dragMoveEvent(QDragMoveEvent* event) override {
        if (IsAssetDrag(event->mimeData())) {
            event->acceptProposedAction();
            return;
        }
        QListWidget::dragMoveEvent(event);
    }

    void dropEvent(QDropEvent* event) override {
        const QMimeData* mime = event->mimeData();
        if (!IsAssetDrag(mime)) {
            QListWidget::dropEvent(event);
            return;
        }
        QString targetDir;
        if (QListWidgetItem* item = itemAt(event->position().toPoint())) {
            const QString path = item->data(Qt::UserRole).toString();
            if (!path.isEmpty() && QFileInfo(path).isDir()) {
                targetDir = path;
            }
        }
        QStringList paths;
        QStringList externalFiles;
        if (mime->hasFormat(QStringLiteral("application/x-cakery-asset"))) {
            const QList<QByteArray> parts = mime->data(
                QStringLiteral("application/x-cakery-asset")).split('\n');
            for (int i = 1; i < parts.size(); i += 2) {
                const QString path = QString::fromUtf8(parts[i]);
                if (!path.isEmpty() && !QFileInfo(path).isDir()) {
                    paths.append(path);
                }
            }
        } else {
            for (const QUrl& url : mime->urls()) {
                if (url.isLocalFile()) {
                    externalFiles.append(url.toLocalFile());
                }
            }
        }
        if (dropHandler) {
            dropHandler(paths, externalFiles, targetDir);
        }
        event->acceptProposedAction();
    }

private:
    static bool IsAssetDrag(const QMimeData* mime) {
        return mime->hasFormat(QStringLiteral("application/x-cakery-asset")) || mime->hasUrls();
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
    m_tree->setAcceptDrops(true);
    m_tree->setDropIndicatorShown(true);
    m_tree->setDragDropMode(QAbstractItemView::DragDrop);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tree->setFocusPolicy(Qt::StrongFocus);
    static_cast<AssetTreeWidget*>(m_tree)->dropHandler =
        [this](const QStringList& paths, const QStringList& externalFiles, const QString& targetDir) {
            handleAssetDrop(paths, externalFiles, targetDir);
        };
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
    m_assetGrid->setAcceptDrops(true);
    m_assetGrid->setDragDropMode(QAbstractItemView::DragDrop);
    m_assetGrid->setContextMenuPolicy(Qt::CustomContextMenu);
    m_assetGrid->setSelectionMode(QAbstractItemView::ExtendedSelection);
    static_cast<AssetGridWidget*>(m_assetGrid)->dropHandler =
        [this](const QStringList& paths, const QStringList& externalFiles, const QString& targetDir) {
            handleAssetDrop(paths, externalFiles, targetDir);
        };

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
            m_context.session().selection().clear();
            emit assetSelectionCleared();
            return;
        }
        const std::uint64_t guid = current->data(0, Qt::UserRole + 3).toULongLong();
        for (const auto& asset : m_assets) {
            if (asset.uuid == guid) {
                m_context.session().selection().setAsset(asset.uuid);
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
    connect(m_assetGrid, &QListWidget::customContextMenuRequested, this, &ProjectPanel::onGridContextMenu);
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
    const auto addSelectAllShortcut = [](QWidget* target, auto&& slot) {
        auto* shortcut = new QShortcut(QKeySequence::SelectAll, target);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(shortcut, &QShortcut::activated, target, slot);
    };
    addSelectAllShortcut(m_tree, &QTreeWidget::selectAll);
    addSelectAllShortcut(m_assetGrid, &QListWidget::selectAll);

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
        constexpr std::size_t kMaxWatchedDirectories = 2048;
        for (std::filesystem::recursive_directory_iterator it(m_root, watcherEc), end;
             it != end && directories.size() < kMaxWatchedDirectories; it.increment(watcherEc)) {
            if (!watcherEc && it->is_directory(watcherEc)) {
                directories.push_back(QString::fromStdString(it->path().string()));
            }
            watcherEc.clear();
        }
        directories.push_back(QString::fromStdString(m_root.string()));
        const QStringList failed = m_watcher->addPaths(directories);
        if (!failed.isEmpty()) {
            qWarning() << "Cakery: failed to watch" << failed.size() << "asset directories";
        }
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

QStringList ProjectPanel::selectedTreePaths() const
{
    QStringList paths;
    const QList<QTreeWidgetItem*> selected = m_tree->selectedItems();
    for (QTreeWidgetItem* item : selected) {
        if (item->data(0, Qt::UserRole + 1).toBool()) {
            continue;
        }
        const QString path = item->data(0, Qt::UserRole).toString();
        if (!path.isEmpty()) {
            paths << path;
        }
    }
    return paths;
}

QStringList ProjectPanel::selectedGridPaths() const
{
    QStringList paths;
    const QList<QListWidgetItem*> selected = m_assetGrid->selectedItems();
    for (QListWidgetItem* item : selected) {
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty() && !QFileInfo(path).isDir()) {
            paths << path;
        }
    }
    return paths;
}

QStringList ProjectPanel::findAssetReferenceHolders(std::uint64_t guid) const
{
    const std::string needle = "\"asset_id\":" + std::to_string(guid);
    QStringList holders;
    for (const EditorEntity& entity : m_context.session().documentModel().entities()) {
        bool references = false;
        for (const EditorComponent& component : entity.nativeComponents) {
            if (component.value.dump().find(needle) != std::string::npos) {
                references = true;
                break;
            }
        }
        if (!references) {
            for (const EditorComponent& component : entity.managedComponents) {
                if (component.value.dump().find(needle) != std::string::npos) {
                    references = true;
                    break;
                }
            }
        }
        if (references) {
            holders << QString::fromStdString(entity.name);
        }
    }
    return holders;
}

void ProjectPanel::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (item && !item->isSelected()) {
        m_tree->clearSelection();
        m_tree->setCurrentItem(item);
        item->setSelected(true);
    }
    openAssetMenu(selectedTreePaths(), m_tree->viewport()->mapToGlobal(pos));
}

void ProjectPanel::onGridContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = m_assetGrid->itemAt(pos);
    if (item && !item->isSelected()) {
        m_assetGrid->clearSelection();
        m_assetGrid->setCurrentItem(item);
        item->setSelected(true);
    }
    openAssetMenu(selectedGridPaths(), m_assetGrid->viewport()->mapToGlobal(pos));
}

void ProjectPanel::openAssetMenu(const QStringList& paths, const QPoint& globalPos)
{
    QMenu menu(this);
    QAction* newSceneAction = menu.addAction(tr("New Scene"));
    QAction* newFolderAction = menu.addAction(tr("New Folder"));
    QAction* importAssetAction = menu.addAction(tr("Import Asset..."));
    menu.addSeparator();
    QAction* reimportAssetAction = nullptr;
    QAction* duplicateAssetAction = nullptr;
    QAction* renameAssetAction = nullptr;
    QAction* deleteAssetAction = nullptr;
    QAction* revealAssetAction = nullptr;
    const bool hasAssets = !paths.isEmpty();
    if (hasAssets) {
        reimportAssetAction = menu.addAction(paths.size() > 1 ? tr("Reimport Assets") : tr("Reimport Asset"));
        duplicateAssetAction = menu.addAction(paths.size() > 1 ? tr("Duplicate Assets") : tr("Duplicate"));
        renameAssetAction = menu.addAction(tr("Rename"));
        deleteAssetAction = menu.addAction(paths.size() > 1 ? tr("Delete Assets") : tr("Delete"));
        revealAssetAction = menu.addAction(tr("Show in Explorer"));
        menu.addSeparator();
    }
    QAction* refreshAction = menu.addAction(tr("Refresh"));
    QAction* chosen = menu.exec(globalPos);
    if (!chosen) {
        return;
    }
    if (chosen == newSceneAction) {
        onNewScene();
    } else if (chosen == newFolderAction) {
        onNewFolder();
    } else if (chosen == importAssetAction) {
        onImportAsset();
    } else if (chosen == reimportAssetAction) {
        reimportAssets(paths);
    } else if (chosen == duplicateAssetAction) {
        duplicateAssets(paths);
    } else if (chosen == renameAssetAction) {
        renameAsset(paths.first());
    } else if (chosen == deleteAssetAction) {
        deleteAssets(paths);
    } else if (chosen == revealAssetAction) {
        revealAssets(paths);
    } else if (chosen == refreshAction) {
        refresh();
    }
}

void ProjectPanel::handleAssetDrop(const QStringList& paths, const QStringList& externalFiles,
                                   const QString& targetDir)
{
    std::filesystem::path dir = targetDir.isEmpty()
        ? m_gridDirectory : std::filesystem::path(targetDir.toStdString());
    if (dir.empty()) {
        dir = m_root;
    }
    if (!paths.isEmpty()) {
        moveAssetsTo(paths, dir);
    }
    if (!externalFiles.isEmpty()) {
        importExternalFiles(externalFiles, dir);
    }
}

void ProjectPanel::moveAssetsTo(const QStringList& paths, const std::filesystem::path& targetDir)
{
    if (paths.isEmpty() || targetDir.empty()) {
        return;
    }
    const std::string target = normalizedPath(targetDir);
    bool moved = false;
    for (const QString& path : paths) {
        QFileInfo info(path);
        if (!info.isFile()) {
            continue;
        }
        if (normalizedPath(std::filesystem::path(info.absolutePath().toStdString())) == target) {
            continue;
        }
        const QString newPath = QString::fromStdString(
            (targetDir / info.fileName().toStdString()).string());
        if (QFileInfo::exists(newPath)) {
            continue;
        }
        const QString oldMeta = path + QStringLiteral(".meta");
        const QString newMeta = newPath + QStringLiteral(".meta");
        const bool hasMeta = QFileInfo::exists(oldMeta);
        if (hasMeta && QFileInfo::exists(newMeta)) {
            continue;
        }
        if (!QFile::rename(path, newPath)) {
            continue;
        }
        if (hasMeta) {
            QFile::rename(oldMeta, newMeta);
        }
        m_context.session().execute({"asset.import", newPath.toStdString()});
        moved = true;
    }
    if (moved) {
        refresh();
    }
}

void ProjectPanel::importExternalFiles(const QStringList& files, const std::filesystem::path& targetDir)
{
    if (files.isEmpty() || targetDir.empty()) {
        return;
    }
    bool imported = false;
    for (const QString& source : files) {
        QFileInfo info(source);
        if (!info.isFile()) {
            continue;
        }
        const QString destination = QString::fromStdString(
            (targetDir / info.fileName().toStdString()).string());
        if (info.absoluteFilePath() == QFileInfo(destination).absoluteFilePath()) {
            m_context.session().execute({"asset.import", destination.toStdString()});
            imported = true;
            continue;
        }
        if (QFileInfo::exists(destination)) {
            continue;
        }
        QFile sourceFile(source);
        QSaveFile destinationFile(destination);
        if (sourceFile.open(QIODevice::ReadOnly) && destinationFile.open(QIODevice::WriteOnly) &&
            destinationFile.write(sourceFile.readAll()) >= 0 && destinationFile.commit()) {
            m_context.session().execute({"asset.import", destination.toStdString()});
            imported = true;
        }
    }
    if (imported) {
        refresh();
    }
}

void ProjectPanel::reimportAssets(const QStringList& paths)
{
    bool reimported = false;
    for (const QString& path : paths) {
        if (!m_context.session().execute({"asset.reimport", path.toStdString()})) {
            QMessageBox::warning(this, tr("Reimport Asset"),
                                 tr("The asset could not be reimported:\n%1").arg(path));
        } else {
            reimported = true;
        }
    }
    if (reimported) {
        refresh();
    }
}

void ProjectPanel::duplicateAssets(const QStringList& paths)
{
    bool duplicated = false;
    for (const QString& path : paths) {
        QFileInfo info(path);
        if (!info.isFile()) {
            continue;
        }
        const QString suffix = info.completeSuffix().isEmpty()
            ? QString()
            : QStringLiteral(".") + info.completeSuffix();
        const QString base = info.completeBaseName();
        QString target;
        for (int index = 1; index < 1000; ++index) {
            const QString candidate = info.absoluteDir().filePath(
                base + QStringLiteral(" ") + QString::number(index) + suffix);
            if (!QFileInfo::exists(candidate)) {
                target = candidate;
                break;
            }
        }
        if (target.isEmpty() || !QFile::copy(path, target)) {
            continue;
        }
        m_context.session().execute({"asset.import", target.toStdString()});
        duplicated = true;
    }
    if (duplicated) {
        refresh();
    }
}

void ProjectPanel::renameAsset(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }
    const QFileInfo oldInfo(path);
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Rename Asset"), tr("Asset name:"), QLineEdit::Normal,
        oldInfo.completeBaseName(), &ok).trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    const QString suffix = oldInfo.completeSuffix().isEmpty()
        ? QString()
        : QStringLiteral(".") + oldInfo.completeSuffix();
    const QString newPath = oldInfo.absoluteDir().filePath(name + suffix);
    if (newPath == path) {
        return;
    }
    if (QFileInfo::exists(newPath) || !QFile::rename(path, newPath)) {
        QMessageBox::warning(this, tr("Rename Asset"), tr("Could not rename the asset."));
        return;
    }
    const QString oldMeta = path + QStringLiteral(".meta");
    const QString newMeta = newPath + QStringLiteral(".meta");
    const bool metaMoved = QFileInfo::exists(oldMeta) && QFile::rename(oldMeta, newMeta);
    if (!m_context.session().execute({"asset.import", newPath.toStdString()})) {
        QFile::rename(newPath, path);
        if (metaMoved) {
            QFile::rename(newMeta, oldMeta);
        }
        QMessageBox::warning(this, tr("Rename Asset"),
                             tr("The asset importer rejected the renamed file; the rename was reverted."));
        return;
    }
    refresh();
}

void ProjectPanel::deleteAssets(const QStringList& paths)
{
    if (paths.isEmpty()) {
        return;
    }
    QStringList referenced;
    for (const QString& path : paths) {
        const auto normalized = normalizedPath(std::filesystem::path(path.toStdString()));
        for (const auto& asset : m_assets) {
            if (normalizedPath(std::filesystem::path(asset.path)) != normalized) {
                continue;
            }
            const QStringList holders = findAssetReferenceHolders(asset.uuid);
            if (!holders.isEmpty()) {
                referenced << tr("%1 (used by: %2)")
                    .arg(QString::fromStdString(asset.name), holders.join(QStringLiteral(", ")));
            }
            break;
        }
    }
    QString message = paths.size() == 1
        ? tr("Delete the selected asset from the project?")
        : tr("Delete %1 selected assets from the project?").arg(paths.size());
    if (!referenced.isEmpty()) {
        message += QStringLiteral("\n\n") + tr("Referenced assets will break:") +
                   QStringLiteral("\n") + referenced.join(QStringLiteral("\n"));
    }
    if (QMessageBox::question(this, tr("Delete Asset"), message,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    QStringList failed;
    for (const QString& path : paths) {
        if (!QFile::remove(path)) {
            failed << path;
            continue;
        }
        const QString metaPath = path + QStringLiteral(".meta");
        if (QFileInfo::exists(metaPath)) {
            QFile::remove(metaPath);
        }
    }
    if (!failed.isEmpty()) {
        QMessageBox::warning(this, tr("Delete Asset"),
                             tr("Could not delete:\n%1").arg(failed.join(QStringLiteral("\n"))));
    }
    refresh();
}

void ProjectPanel::revealAssets(const QStringList& paths)
{
    QStringList revealed;
    for (const QString& path : paths) {
        const QString dir = QFileInfo(path).absolutePath();
        if (dir.isEmpty() || revealed.contains(dir)) {
            continue;
        }
        revealed << dir;
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    }
}

void ProjectPanel::onImportAsset()
{
    const QString source = QFileDialog::getOpenFileName(
        this, tr("Import Asset"), QString(),
        tr("Assets (*.png *.jpg *.jpeg *.bmp *.gif *.tga *.psd *.hdr *.obj *.fbx *.gltf *.glb "
           "*.tmj *.tsx *.wav *.ogg *.mp3 *.flac *.shader *.cs *.prefab);;All Files (*)"));
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
    m_gridDirectory = directory;
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
