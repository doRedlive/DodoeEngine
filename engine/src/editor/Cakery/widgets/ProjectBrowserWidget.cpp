#include "ProjectBrowserWidget.h"

#include "runtime/core/project/project.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QHeaderView>
#include <QIcon>
#include <QCoreApplication>

namespace cakery {

static QString resPath(const QString& relative)
{
    QStringList candidates;
    candidates << QCoreApplication::applicationDirPath() + "/resources/" + relative;
    candidates << QCoreApplication::applicationDirPath() + "/../engine/res/" + relative;
    candidates << QCoreApplication::applicationDirPath() + "/../../engine/res/" + relative;
    for (const auto& p : candidates) {
        if (QFileInfo::exists(p)) return p;
    }
    return QString();
}

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
    toolbar->addWidget(m_createBtn);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search Assets..."));
    toolbar->addWidget(m_searchEdit, 1);

    layout->addLayout(toolbar);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(3);

    m_folderTree = new QTreeWidget(this);
    m_folderTree->setHeaderHidden(true);
    m_folderTree->setMinimumWidth(150);
    m_folderTree->setMaximumWidth(320);
    m_splitter->addWidget(m_folderTree);

    auto* rightPane = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(2);

    m_breadcrumb = new QLabel(this);
    m_breadcrumb->setStyleSheet("background:#21222C; color:#BC92F9; padding:5px 9px; border-radius:6px;");
    rightLayout->addWidget(m_breadcrumb);

    m_assetList = new QListWidget(this);
    m_assetList->setViewMode(QListView::IconMode);
    m_assetList->setIconSize(QSize(48, 48));
    m_assetList->setGridSize(QSize(84, 78));
    m_assetList->setSpacing(8);
    m_assetList->setResizeMode(QListView::Adjust);
    m_assetList->setMovement(QListView::Static);
    m_assetList->setWordWrap(true);
    m_assetList->setTextElideMode(Qt::ElideRight);
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
    footer->addWidget(m_iconSizeSlider);

    layout->addLayout(footer);

    connect(m_folderTree, &QTreeWidget::itemClicked, this, &ProjectBrowserWidget::onFolderSelected);
    connect(m_assetList, &QListWidget::itemDoubleClicked,
            this, &ProjectBrowserWidget::onAssetDoubleClicked);
    connect(m_iconSizeSlider, &QSlider::valueChanged,
            this, &ProjectBrowserWidget::onIconSizeChanged);
}

void ProjectBrowserWidget::setBasePath(const QString& path)
{
    m_basePath = path;
    m_currentFolder = path;
    m_pathLabel->setText(path);
    refresh();
}

void ProjectBrowserWidget::refresh()
{
    buildFolderTree(m_basePath);
    populateAssets(m_currentFolder.isEmpty() ? m_basePath : m_currentFolder);
    m_breadcrumb->setText(m_currentFolder.isEmpty() ? "Assets" : m_currentFolder);
}

void ProjectBrowserWidget::buildFolderTree(const QString& rootPath)
{
    m_folderTree->clear();

    QDir rootDir(rootPath);
    if (!rootDir.exists()) return;

    QIcon dirIcon(resPath("pictures/ContentBrowser/DirectoryIcon.png"));

    std::function<void(QTreeWidgetItem*, const QDir&)> addDirs =
        [&](QTreeWidgetItem* parent, const QDir& dir) {
        for (const auto& info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            auto* item = new QTreeWidgetItem(parent);
            item->setText(0, info.fileName());
            item->setData(0, Qt::UserRole, info.absoluteFilePath());
            if (!dirIcon.isNull()) item->setIcon(0, dirIcon);
            addDirs(item, QDir(info.absoluteFilePath()));
        }
    };

    for (const auto& info : rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        auto* item = new QTreeWidgetItem(m_folderTree);
        item->setText(0, info.fileName());
        item->setData(0, Qt::UserRole, info.absoluteFilePath());
        if (!dirIcon.isNull()) item->setIcon(0, dirIcon);
        item->setExpanded(true);
        addDirs(item, QDir(info.absoluteFilePath()));
    }
}

void ProjectBrowserWidget::populateAssets(const QString& folderPath)
{
    m_assetList->clear();
    m_currentFolder = folderPath;

    QDir dir(folderPath);
    if (!dir.exists()) return;

    QIcon fileIcon(resPath("pictures/ContentBrowser/FileIcon.png"));

    for (const auto& info : dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        auto* item = new QListWidgetItem(info.fileName());
        item->setData(Qt::UserRole, info.absoluteFilePath());
        if (!fileIcon.isNull()) item->setIcon(fileIcon);
        m_assetList->addItem(item);
    }
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
    QString path = item->data(Qt::UserRole).toString();
    if (QFileInfo::exists(path)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void ProjectBrowserWidget::onIconSizeChanged(int value)
{
    m_assetList->setIconSize(QSize(value, value));
    m_assetList->setGridSize(QSize(value + 36, value + 30));
}

} // namespace cakery
