// do@Redlive

#include "ProjectPanel.h"

#include "cakery/ui/EditorWorkspaceContext.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QTreeWidget>
#include <QSize>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace cakery {

namespace {

QIcon contentBrowserIcon(const QString& fileName)
{
    const QString path = QDir(QApplication::applicationDirPath())
        .filePath(QStringLiteral("resources/pictures/ContentBrowser/") + fileName);
    return QIcon(path);
}

} // namespace

ProjectPanel::ProjectPanel(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent), m_context(context)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setIconSize(QSize(16, 16));
    m_tree->setIndentation(16);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &ProjectPanel::onDocumentDoubleClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &ProjectPanel::onContextMenu);
    refresh();
}

void ProjectPanel::refresh()
{
    m_context.session().execute(EditorCommandMessage{"asset.refresh", {}});
    m_tree->clear();
    m_root = m_context.session().assetRoot();
    if (m_root.empty()) {
        auto* item = new QTreeWidgetItem(m_tree);
        item->setText(0, tr("No project open"));
        return;
    }

    auto* rootItem = new QTreeWidgetItem(m_tree);
    rootItem->setText(0, QFileInfo(QString::fromStdString(m_root.string())).fileName());
    rootItem->setIcon(0, contentBrowserIcon(QStringLiteral("DirectoryIcon.png")));
    rootItem->setData(0, Qt::UserRole, QString::fromStdString(m_root.string()));
    rootItem->setData(0, Qt::UserRole + 1, true);
    addDirectory(rootItem, m_root);
    rootItem->setExpanded(true);
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
        auto* item = new QTreeWidgetItem(parentItem);
        item->setText(0, QString::fromStdString(dir.filename().string()));
        item->setIcon(0, contentBrowserIcon(QStringLiteral("DirectoryIcon.png")));
        item->setData(0, Qt::UserRole, QString::fromStdString(dir.string()));
        item->setData(0, Qt::UserRole + 1, true);
        addDirectory(item, dir);
    }
    for (const auto& file : files) {
        auto* item = new QTreeWidgetItem(parentItem);
        item->setText(0, QString::fromStdString(file.filename().string()));
        item->setIcon(0, contentBrowserIcon(QStringLiteral("FileIcon.png")));
        item->setData(0, Qt::UserRole, QString::fromStdString(file.string()));
        item->setData(0, Qt::UserRole + 1, false);
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
    menu.addSeparator();
    QAction* refreshAction = menu.addAction(tr("Refresh"));
    QAction* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (chosen == newSceneAction) {
        onNewScene();
    } else if (chosen == newFolderAction) {
        onNewFolder();
    } else if (chosen == refreshAction) {
        refresh();
    }
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
