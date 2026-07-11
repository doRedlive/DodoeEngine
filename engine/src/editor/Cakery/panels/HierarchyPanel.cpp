// do@Redlive

#include "HierarchyPanel.h"
#include "framework/EditorContext.h"
#include "framework/selection/SelectionManager.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/id_component.h"
#include "runtime/function/world/components/hierarchy_component.h"

#include <QVBoxLayout>

namespace cakery {

static const int kEntityIdRole = Qt::UserRole + 1;

HierarchyPanel::HierarchyPanel(EditorContext& ctx, QWidget* parent)
    : Panel(ctx, parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &HierarchyPanel::onItemSelected);
}

void HierarchyPanel::refresh()
{
    m_tree->clear();

    auto* scene = m_ctx.activeScene();
    if (!scene) return;

    auto entities = scene->getEntities();
    if (entities.empty()) return;

    for (auto& entity : entities) {
        if (!entity.valid()) continue;
        auto* item = new QTreeWidgetItem();
        item->setText(0, QString::fromStdString(entity.name()));
        item->setData(0, kEntityIdRole, QVariant::fromValue(static_cast<quint64>(entity.handle())));
        m_tree->addTopLevelItem(item);
    }
}

void HierarchyPanel::onItemSelected()
{
    auto* item = m_tree->currentItem();
    if (!item) {
        m_ctx.selection().clear();
        return;
    }

    auto* scene = m_ctx.activeScene();
    if (!scene) return;

    auto id = static_cast<entt::entity>(item->data(0, kEntityIdRole).value<quint64>());
    dodoe::Entity entity(scene, id);
    if (entity.valid()) {
        m_ctx.selection().select(entity.uuid());
    }
}

} // namespace cakery
