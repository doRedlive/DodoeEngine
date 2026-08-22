// do@Redlive

#include "ProjectPanel.h"

#include "cakery/ui/EditorWorkspaceContext.h"

#include <QFileInfo>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <filesystem>
#include <string>

namespace cakery {

ProjectPanel::ProjectPanel(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent), m_context(context)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &ProjectPanel::onDocumentDoubleClicked);
    refresh();
}

void ProjectPanel::refresh()
{
    const std::string root = m_context.session().project().rootPath;
    m_tree->clear();
    if (root.empty()) {
        auto* item = new QTreeWidgetItem(m_tree);
        item->setText(0, tr("No project open"));
        return;
    }

    auto* rootItem = new QTreeWidgetItem(m_tree);
    rootItem->setText(0, QFileInfo(QString::fromStdString(root)).fileName());

    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(
        root, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec)) {
            continue;
        }
        const std::filesystem::path path = it->path();
        if (path.extension() != ".doscn") {
            continue;
        }
        auto* child = new QTreeWidgetItem(rootItem);
        child->setText(0, QString::fromStdString(path.filename().string()));
        child->setData(0, Qt::UserRole, QString::fromStdString(path.string()));
    }
    rootItem->setExpanded(true);
}

void ProjectPanel::onDocumentDoubleClicked(QTreeWidgetItem* item, int column)
{
    (void)column;
    if (!item) {
        return;
    }
    const QString path = item->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) {
        return;
    }
    m_context.session().openDocument(path.toStdString());
}

} // namespace cakery
