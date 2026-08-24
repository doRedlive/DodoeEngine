// do@Redlive

#include "HierarchyPanel.h"

#include "cakery/ui/EditorWorkspaceContext.h"

#include <QMenu>
#include <QSignalBlocker>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace cakery {

namespace {

std::uint64_t ItemUuid(QTreeWidgetItem* item) {
    return item ? item->data(0, Qt::UserRole).toString().toULongLong() : 0;
}

} // namespace

HierarchyPanel::HierarchyPanel(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent), m_context(context)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &HierarchyPanel::onTreeSelectionChanged);
    connect(m_tree, &QTreeWidget::itemChanged, this, &HierarchyPanel::onItemEdited);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &HierarchyPanel::onContextMenu);

    m_documentSubscription = m_context.session().documentModel().subscribe([this]() { refresh(); });
    m_selectionSubscription = m_context.session().selection().subscribe([this]() { refreshSelection(); });
    refresh();
}

void HierarchyPanel::refresh()
{
    const auto& entities = m_context.session().documentModel().entities();
    const std::uint64_t selected = m_context.session().selection().selected();

    QSignalBlocker blocker(m_tree);
    m_tree->clear();
    for (const auto& entity : entities) {
        auto* item = new QTreeWidgetItem();
        item->setText(0, QString::fromStdString(entity.name));
        item->setData(0, Qt::UserRole, QString::number(entity.uuid));
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        if (entity.uuid == selected) {
            item->setSelected(true);
        }
        m_tree->addTopLevelItem(item);
    }
    blocker.unblock();

    if (selected != 0 && !m_context.session().documentModel().findEntity(selected)) {
        m_context.session().selection().clear();
    }
}

void HierarchyPanel::refreshSelection()
{
    const std::uint64_t selected = m_context.session().selection().selected();
    QSignalBlocker blocker(m_tree);
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_tree->topLevelItem(i);
        item->setSelected(ItemUuid(item) == selected);
    }
    blocker.unblock();
}

void HierarchyPanel::onTreeSelectionChanged()
{
    QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) {
        m_context.session().selection().clear();
        return;
    }
    m_context.session().selection().set(ItemUuid(items.first()));
}

void HierarchyPanel::onItemEdited(QTreeWidgetItem* item, int column)
{
    if (column != 0 || !item) {
        return;
    }
    const std::uint64_t uuid = ItemUuid(item);
    const QString name = item->text(0);
    QTimer::singleShot(0, this, [this, uuid, name]() {
        m_context.session().renameEntity(uuid, name.toStdString());
    });
}

void HierarchyPanel::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    QMenu menu(this);
    QAction* create = menu.addAction(tr("Create Entity"));
    QAction* rename = nullptr;
    QAction* remove = nullptr;
    if (item) {
        menu.addSeparator();
        rename = menu.addAction(tr("Rename"));
        remove = menu.addAction(tr("Delete"));
    }

    QAction* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (chosen == create) {
        onCreateEntity();
    } else if (chosen == rename) {
        m_tree->editItem(item, 0);
    } else if (chosen == remove) {
        onDeleteEntity();
    }
}

void HierarchyPanel::onCreateEntity()
{
    m_context.session().createEntity(tr("New Entity").toStdString());
}

void HierarchyPanel::onDeleteEntity()
{
    QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) {
        return;
    }
    m_context.session().deleteEntity(ItemUuid(items.first()));
}

} // namespace cakery
