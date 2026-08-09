// do@Redlive

#include "ProjectPanel.h"
#include "framework/EditorContext.h"
#include "framework/asset/AssetDatabase.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDir>
#include <QFileInfo>
#include <QIcon>

namespace cakery {

ProjectPanel::ProjectPanel(EditorContext& ctx, QWidget* parent)
    : Panel(ctx, parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);
    layout->setSpacing(2);

    auto* toolbar = new QHBoxLayout();
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText("Search...");
    m_searchBox->setClearButtonEnabled(true);
    toolbar->addWidget(m_searchBox, 1);

    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItems({"All", "Scenes", "Textures", "Meshes", "Scripts"});
    toolbar->addWidget(m_filterCombo);
    layout->addLayout(toolbar);

    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_dirTree = new QTreeWidget(this);
    m_dirTree->setHeaderLabels({"Name"});
    m_dirTree->header()->setStretchLastSection(true);
    m_splitter->addWidget(m_dirTree);

    m_assetGrid = new QListWidget(this);
    m_assetGrid->setViewMode(QListView::IconMode);
    m_assetGrid->setIconSize(QSize(64, 64));
    m_assetGrid->setGridSize(QSize(80, 80));
    m_assetGrid->setResizeMode(QListView::Adjust);
    m_assetGrid->setWordWrap(true);
    m_splitter->addWidget(m_assetGrid);

    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 2);
    layout->addWidget(m_splitter, 1);
}

void ProjectPanel::refresh()
{
    populateFromAssetDatabase();
}

void ProjectPanel::setBasePath(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) return;

    m_dirTree->clear();
    auto* rootItem = new QTreeWidgetItem();
    rootItem->setText(0, dir.dirName());
    m_dirTree->addTopLevelItem(rootItem);

    populateFromAssetDatabase();
}

void ProjectPanel::populateFromAssetDatabase()
{
    m_assetGrid->clear();
    auto& db = m_ctx.assets();
    auto entries = db.list();

    for (auto& entry : entries) {
        QFileInfo fi(QString::fromStdString(entry.path));
        auto* item = new QListWidgetItem();
        item->setText(fi.fileName());
        if (entry.type == "texture") {
            item->setIcon(QIcon(fi.filePath()));
        }
        item->setToolTip(QString("%1\n%2").arg(fi.fileName(), QString::fromStdString(entry.type)));
        m_assetGrid->addItem(item);
    }
}

} // namespace cakery
