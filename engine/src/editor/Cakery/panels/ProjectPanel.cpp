// do@Redlive

#include "ProjectPanel.h"
#include "framework/EditorContext.h"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QDir>

namespace cakery {

ProjectPanel::ProjectPanel(EditorContext& ctx, QWidget* parent)
    : Panel(ctx, parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({tr("Name"), tr("Type")});
    m_tree->header()->setStretchLastSection(false);
    layout->addWidget(m_tree);
}

void ProjectPanel::setBasePath(const QString& path)
{
    m_tree->clear();
    QDir dir(path);
    for (auto& fi : dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        auto* item = new QTreeWidgetItem();
        item->setText(0, fi.fileName());
        item->setText(1, fi.isDir() ? "Folder" : "File");
        m_tree->addTopLevelItem(item);
    }
}

} // namespace cakery
