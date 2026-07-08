#include "ProjectBrowserWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QHeaderView>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QCoreApplication>
#include <QApplication>
#include <QStyle>
#include <QMimeData>

namespace cakery {

ProjectBrowserWidget::ProjectBrowserWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);
    layout->setSpacing(3);

    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(3);

    m_createBtn = new QToolButton(this);
    m_createBtn->setText(QString::fromUtf8("＋ Create"));
    m_createBtn->setToolTip(tr("Create Asset"));
    m_createBtn->setPopupMode(QToolButton::InstantPopup);
    auto* createMenu = new QMenu(this);
    createMenu->addAction(tr("Folder"), this, &ProjectBrowserWidget::onCreateFolder);
    m_createBtn->setMenu(createMenu);
    toolbar->addWidget(m_createBtn);

    m_refreshBtn = new QToolButton(this);
    m_refreshBtn->setText(tr("Refresh"));
    m_refreshBtn->setToolTip(tr("Refresh"));
    connect(m_refreshBtn, &QToolButton::clicked, this, &ProjectBrowserWidget::onRefresh);
    toolbar->addWidget(m_refreshBtn);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search Assets..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ProjectBrowserWidget::onSearchChanged);
    toolbar->addWidget(m_searchEdit, 1);

    layout->addLayout(toolbar);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(3);

    m_folderTree = new QTreeWidget(this);
    m_folderTree->setHeaderHidden(true);
    m_folderTree->setMinimumWidth(150);
    m_folderTree->setMaximumWidth(320);
    m_folderTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_folderTree, &QTreeWidget::itemClicked, this, &ProjectBrowserWidget::onFolderSelected);
    connect(m_folderTree, &QTreeWidget::customContextMenuRequested, this, &ProjectBrowserWidget::onTreeContextMenu);
    m_splitter->addWidget(m_folderTree);

    auto* rightPane = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(2);

    m_breadcrumb = new QLabel(this);
    m_breadcrumb->setStyleSheet("background:#21222C; color:#BC92F9; padding:5px 9px; border-radius:6px;");
    rightLayout->addWidget(m_breadcrumb);

    m_assetList = new AssetListWidget(this);
    m_assetList->setViewMode(QListView::IconMode);
    m_assetList->setIconSize(QSize(48, 48));
    m_assetList->setGridSize(QSize(84, 78));
    m_assetList->setSpacing(8);
    m_assetList->setResizeMode(QListView::Adjust);
    m_assetList->setMovement(QListView::Static);
    m_assetList->setWordWrap(true);
    m_assetList->setTextElideMode(Qt::ElideRight);
    m_assetList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_assetList, &QListWidget::itemDoubleClicked, this, &ProjectBrowserWidget::onAssetDoubleClicked);
    connect(m_assetList, &QListWidget::customContextMenuRequested, this, &ProjectBrowserWidget::onAssetContextMenu);
    rightLayout->addWidget(m_assetList, 1);

    m_splitter->addWidget(rightPane);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({200, 400});

    layout->addWidget(m_splitter, 1);

    auto* footer = new QHBoxLayout();
    footer->setSpacing(6);

    m_pathLabel = new QLabel(this);
    m_pathLabel->setStyleSheet("color:#6272A4;");
    footer->addWidget(m_pathLabel);

    footer->addStretch();

    m_iconSizeSlider = new QSlider(Qt::Horizontal, this);
    m_iconSizeSlider->setMinimumSize(90, 0);
    m_iconSizeSlider->setMaximumSize(120, 16777215);
    m_iconSizeSlider->setRange(16, 96);
    m_iconSizeSlider->setValue(48);
    connect(m_iconSizeSlider, &QSlider::valueChanged, this, &ProjectBrowserWidget::onIconSizeChanged);
    footer->addWidget(m_iconSizeSlider);

    layout->addLayout(footer);
}

void ProjectBrowserWidget::setBasePath(const QString& path)
{
    m_basePath = QDir::toNativeSeparators(path);
    m_currentFolder = m_basePath;
    m_pathLabel->setText(m_basePath);
    refresh();
}

void ProjectBrowserWidget::refresh()
{
    buildFolderTree(m_basePath);
    populateAssets(m_currentFolder.isEmpty() ? m_basePath : m_currentFolder);
}

void ProjectBrowserWidget::buildFolderTree(const QString& rootPath)
{
    m_folderTree->clear();

    QDir rootDir(rootPath);
    if (!rootDir.exists()) return;

    auto* rootItem = new QTreeWidgetItem(m_folderTree);
    rootItem->setText(0, QFileInfo(rootPath).fileName());
    rootItem->setData(0, Qt::UserRole, rootPath);
    rootItem->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    rootItem->setExpanded(true);

    std::function<void(QTreeWidgetItem*, const QDir&)> addDirs =
        [&](QTreeWidgetItem* parent, const QDir& dir) {
        for (const auto& info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            auto* item = new QTreeWidgetItem(parent);
            item->setText(0, info.fileName());
            item->setData(0, Qt::UserRole, info.absoluteFilePath());
            item->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
            addDirs(item, QDir(info.absoluteFilePath()));
        }
    };

    addDirs(rootItem, rootDir);

    for (int i = 0; i < rootItem->childCount(); ++i) {
        rootItem->child(i)->setExpanded(true);
    }
    m_folderTree->setCurrentItem(rootItem);
}

void ProjectBrowserWidget::populateAssets(const QString& folderPath)
{
    m_assetList->clear();
    m_currentFolder = folderPath;

    QDir dir(folderPath);
    if (!dir.exists()) return;

    for (const auto& info : dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        auto* item = new QListWidgetItem(info.fileName());
        item->setData(Qt::UserRole, info.absoluteFilePath());
        item->setIcon(iconForFile(info.fileName()));

        QString search = m_searchEdit->text().trimmed();
        if (!search.isEmpty() && !info.fileName().contains(search, Qt::CaseInsensitive)) {
            item->setHidden(true);
        }

        m_assetList->addItem(item);
    }

    m_breadcrumb->setText(QDir(m_basePath).relativeFilePath(m_currentFolder));
}

void ProjectBrowserWidget::onFolderSelected()
{
    auto* item = m_folderTree->currentItem();
    if (!item) return;

    QString folderPath = item->data(0, Qt::UserRole).toString();
    if (folderPath.isEmpty()) return;

    QStringList parts;
    auto* p = item;
    while (p) {
        parts.prepend(p->text(0));
        p = p->parent();
    }
    m_breadcrumb->setText(parts.join("  ›  "));

    populateAssets(folderPath);
    m_pathLabel->setText(folderPath);
}

void ProjectBrowserWidget::onAssetDoubleClicked(QListWidgetItem* item)
{
    Q_UNUSED(item);
}

void ProjectBrowserWidget::onIconSizeChanged(int value)
{
    m_assetList->setIconSize(QSize(value, value));
    m_assetList->setGridSize(QSize(value + 36, value + 30));
}

void ProjectBrowserWidget::onCreateFolder()
{
    auto* item = m_folderTree->currentItem();
    QString parentPath = m_currentFolder;
    if (item) {
        parentPath = item->data(0, Qt::UserRole).toString();
    }

    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Create Folder"), tr("Name:"), QLineEdit::Normal, tr("New Folder"), &ok);

    if (ok && !name.isEmpty()) {
        QDir dir(parentPath);
        if (dir.mkdir(name)) {
            refresh();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to create folder."));
        }
    }
}

void ProjectBrowserWidget::onRefresh()
{
    refresh();
}

void ProjectBrowserWidget::onSearchChanged(const QString& text)
{
    for (int i = 0; i < m_assetList->count(); ++i) {
        auto* item = m_assetList->item(i);
        if (text.isEmpty()) {
            item->setHidden(false);
        } else {
            item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
        }
    }
}

void ProjectBrowserWidget::onTreeContextMenu(const QPoint& pos)
{
    auto* item = m_folderTree->itemAt(pos);
    if (!item) return;

    QString path = item->data(0, Qt::UserRole).toString();

    QMenu menu;
    menu.addAction(tr("Create Folder"), this, &ProjectBrowserWidget::onCreateFolder);
    menu.addSeparator();
    menu.addAction(tr("Rename"), this, &ProjectBrowserWidget::onRenameFolder);
    menu.addAction(tr("Delete"), this, &ProjectBrowserWidget::onDeleteFolder);
    menu.addSeparator();
    menu.addAction(tr("Show in Explorer"), this, [this, path] { onShowInExplorer(path); });

    menu.exec(m_folderTree->viewport()->mapToGlobal(pos));
}

void ProjectBrowserWidget::onAssetContextMenu(const QPoint& pos)
{
    auto* item = m_assetList->itemAt(pos);
    if (!item) return;

    QString path = item->data(Qt::UserRole).toString();

    QMenu menu;
    menu.addAction(tr("Show in Explorer"), this, [this, path] { onShowInExplorer(path); });

    menu.exec(m_assetList->viewport()->mapToGlobal(pos));
}

void ProjectBrowserWidget::onRenameFolder()
{
    auto* item = m_folderTree->currentItem();
    if (!item) return;

    QString oldPath = item->data(0, Qt::UserRole).toString();
    QString oldName = item->text(0);

    bool ok = false;
    QString newName = QInputDialog::getText(this, tr("Rename"), tr("Name:"), QLineEdit::Normal, oldName, &ok);

    if (ok && !newName.isEmpty() && newName != oldName) {
        QFileInfo fi(oldPath);
        QString newPath = fi.absoluteDir().absoluteFilePath(newName);
        if (QDir().rename(oldPath, newPath)) {
            refresh();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to rename folder."));
        }
    }
}

void ProjectBrowserWidget::onDeleteFolder()
{
    auto* item = m_folderTree->currentItem();
    if (!item) return;

    QString path = item->data(0, Qt::UserRole).toString();
    QString name = item->text(0);

    auto result = QMessageBox::question(
        this, tr("Delete Folder"),
        tr("Are you sure you want to delete \"%1\"?").arg(name),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        QDir dir(path);
        if (dir.removeRecursively()) {
            refresh();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to delete folder."));
        }
    }
}

void ProjectBrowserWidget::onShowInExplorer(const QString& path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QIcon ProjectBrowserWidget::iconForFile(const QString& fileName) const
{
    QString suffix = QFileInfo(fileName).suffix().toLower();
    return fileTypeIcon(suffix);
}

QIcon ProjectBrowserWidget::fileTypeIcon(const QString& suffix) const
{
    if (suffix == "png" || suffix == "jpg" || suffix == "jpeg" || suffix == "bmp" || suffix == "tga") {
        return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
    }
    if (suffix == "lua" || suffix == "cpp" || suffix == "h" || suffix == "hpp" || suffix == "py") {
        return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
    }
    if (suffix == "doscn") {
        return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
    }
    if (suffix == "doproj") {
        return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
    }
    return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
}

} // namespace cakery
