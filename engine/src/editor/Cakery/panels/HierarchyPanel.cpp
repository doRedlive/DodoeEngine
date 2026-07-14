// do@Redlive

#include "HierarchyPanel.h"
#include "framework/EditorContext.h"
#include "framework/selection/SelectionManager.h"
#include "framework/event/EventBridge.h"
#include "framework/command/CommandStack.h"
#include "framework/command/commands/CreateEntityCommand.h"
#include "framework/command/commands/DeleteEntityCommand.h"
#include "framework/command/commands/ReparentEntityCommand.h"
#include "framework/command/commands/RenameEntityCommand.h"
#include "framework/config/EditorConfig.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/id_component.h"
#include "runtime/function/world/components/hierarchy_component.h"

#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QEvent>
#include <QDrag>
#include <QMimeData>
#include <QDropEvent>

namespace cakery {

static const int kEntityIdRole = Qt::UserRole + 1;
static const int kEntityUuidRole = Qt::UserRole + 2;

HierarchyPanel::HierarchyPanel(EditorContext& ctx, QWidget* parent)
    : Panel(ctx, parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);
    layout->setSpacing(2);

    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText("Search...");
    m_searchBox->setClearButtonEnabled(true);
    layout->addWidget(m_searchBox);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setDragEnabled(true);
    m_tree->setAcceptDrops(true);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree->setDefaultDropAction(Qt::MoveAction);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_tree);

    connect(m_searchBox, &QLineEdit::textChanged, this, &HierarchyPanel::onSearchTextChanged);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &HierarchyPanel::onItemSelected);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &HierarchyPanel::onItemDoubleClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &HierarchyPanel::onCustomContextMenu);
    connect(m_tree, &QTreeWidget::itemChanged, this, &HierarchyPanel::onItemChanged);

    m_tree->installEventFilter(this);

    auto h1 = m_ctx.selection().changed.connect([this](const auto&) {
        if (auto* item = m_tree->currentItem()) {
            auto uuid = dodoe::Uuid(item->data(0, kEntityUuidRole).toULongLong());
            if (uuid == m_ctx.selection().primary()) {
                return;
            }
        }
        auto sel = m_ctx.selection().primary();
        if (sel.valid()) {
            for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
                auto* item = m_tree->topLevelItem(i);
                auto iter = QTreeWidgetItemIterator(item);
                while (*iter) {
                    auto id = dodoe::Uuid((*iter)->data(0, kEntityUuidRole).toULongLong());
                    (*iter)->setSelected(id == sel);
                    if (id == sel) {
                        m_tree->scrollToItem(*iter);
                    }
                    ++iter;
                }
            }
        }
    });
    m_connections.emplace_back(m_ctx.selection().changed, h1);

    auto h2 = m_ctx.events().hierarchyChanged.connect([this](dodoe::Uuid) {
        refresh();
    });
    m_connections.emplace_back(m_ctx.events().hierarchyChanged, h2);

    auto h3 = m_ctx.events().entityCreated.connect([this](dodoe::Uuid) {
        refresh();
    });
    m_connections.emplace_back(m_ctx.events().entityCreated, h3);

    auto h4 = m_ctx.events().entityDestroyed.connect([this](dodoe::Uuid) {
        refresh();
    });
    m_connections.emplace_back(m_ctx.events().entityDestroyed, h4);
}

void HierarchyPanel::refresh()
{
    m_tree->blockSignals(true);
    m_tree->clear();
    populateTree();
    m_tree->blockSignals(false);
}

void HierarchyPanel::populateTree()
{
    auto* scene = m_ctx.activeScene();
    if (!scene) return;

    auto entities = scene->getEntities();
    if (entities.empty()) return;

    std::unordered_map<dodoe::Uuid, QTreeWidgetItem*> itemMap;
    std::vector<std::pair<dodoe::Uuid, QTreeWidgetItem*>> pendingChildren;

    for (auto& entity : entities) {
        if (!entity.valid()) continue;

        auto uuid = entity.uuid();
        if (m_filterText.isEmpty() ||
            QString::fromStdString(entity.name()).contains(m_filterText, Qt::CaseInsensitive)) {

            dodoe::Uuid parentUuid;
            if (entity.hasComponent<dodoe::HierarchyComponent>()) {
                auto& hc = entity.getComponent<dodoe::HierarchyComponent>();
                parentUuid = hc.parent_uuid;
            }

            auto* item = new QTreeWidgetItem();
            item->setText(0, QString::fromStdString(entity.name()));
            item->setData(0, kEntityIdRole, QVariant::fromValue(static_cast<quint64>(entity.handle())));
            item->setData(0, kEntityUuidRole, QVariant::fromValue(static_cast<quint64>(uuid)));
            item->setFlags(item->flags() | Qt::ItemIsEditable);

            itemMap[uuid] = item;

            if (!parentUuid.valid()) {
                m_tree->addTopLevelItem(item);
            } else {
                auto it = itemMap.find(parentUuid);
                if (it != itemMap.end()) {
                    it->second->addChild(item);
                } else {
                    m_tree->addTopLevelItem(item);
                }
            }
        }
    }

    m_tree->expandAll();
}

void HierarchyPanel::addEntityItem(QTreeWidgetItem* parent, dodoe::Uuid uuid, dodoe::Scene* scene)
{
    auto entity = scene->getEntityByUUID(uuid);
    if (!entity.valid()) return;

    auto* item = new QTreeWidgetItem();
    item->setText(0, QString::fromStdString(entity.name()));
    item->setData(0, kEntityIdRole, QVariant::fromValue(static_cast<quint64>(entity.handle())));
    item->setData(0, kEntityUuidRole, QVariant::fromValue(static_cast<quint64>(uuid)));
    item->setFlags(item->flags() | Qt::ItemIsEditable);

    if (parent) {
        parent->addChild(item);
    } else {
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

    auto uuid = dodoe::Uuid(item->data(0, kEntityUuidRole).toULongLong());
    if (uuid.valid()) {
        m_ctx.selection().select(uuid);
    }
}

void HierarchyPanel::onItemDoubleClicked(QTreeWidgetItem* item, int)
{
    if (!item) return;
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    m_tree->editItem(item);
}

void HierarchyPanel::onItemChanged(QTreeWidgetItem* item, int)
{
    if (!item) return;

    auto uuid = dodoe::Uuid(item->data(0, kEntityUuidRole).toULongLong());
    if (!uuid.valid()) return;

    QString newText = item->text(0);
    if (newText.isEmpty()) return;

    auto* scene = m_ctx.activeScene();
    if (!scene) return;

    auto entity = scene->getEntityByUUID(uuid);
    if (!entity.valid()) return;

    std::string oldName = entity.name();
    std::string newName = newText.toStdString();
    if (oldName == newName) return;

    auto cmd = std::make_unique<RenameEntityCommand>(uuid, oldName, newName);
    m_ctx.commands().execute(std::move(cmd));
}

void HierarchyPanel::onCustomContextMenu(const QPoint& pos)
{
    auto* item = m_tree->itemAt(pos);
    dodoe::Uuid uuid;
    if (item) {
        uuid = dodoe::Uuid(item->data(0, kEntityUuidRole).toULongLong());
    }

    QMenu menu(m_tree);

    QAction* createAction = menu.addAction("Create Empty");
    menu.addSeparator();

    QAction* createChildAction = nullptr;
    QAction* duplicateAction = nullptr;
    QAction* renameAction = nullptr;
    QAction* deleteAction = nullptr;

    if (uuid.valid()) {
        createChildAction = menu.addAction("Create Child");
        duplicateAction = menu.addAction("Duplicate");
        renameAction = menu.addAction("Rename");
        menu.addSeparator();
        deleteAction = menu.addAction("Delete");
    }

    QAction* selected = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (!selected) return;

    if (selected == createAction) {
        createEmptyEntity();
    } else if (selected == createChildAction) {
        createEmptyEntity(uuid);
    } else if (selected == duplicateAction) {
        duplicateEntity(uuid);
    } else if (selected == renameAction) {
        renameEntity(item, uuid);
    } else if (selected == deleteAction) {
        deleteEntity(uuid);
    }
}

void HierarchyPanel::onSearchTextChanged(const QString& text)
{
    m_filterText = text;
    refresh();
}

void HierarchyPanel::createEmptyEntity(dodoe::Uuid parentUuid)
{
    dodoe::Uuid newUuid = dodoe::Uuid::Generate();
    auto cmd = std::make_unique<CreateEntityCommand>(newUuid, "GameObject",
        parentUuid.valid() ? std::optional<dodoe::Uuid>(parentUuid) : std::nullopt);
    m_ctx.commands().execute(std::move(cmd));
    m_ctx.events().entityCreated.emit(newUuid);
    m_ctx.selection().select(newUuid);
}

void HierarchyPanel::duplicateEntity(dodoe::Uuid uuid)
{
    dodoe::Uuid newUuid = dodoe::Uuid::Generate();
    auto* scene = m_ctx.activeScene();
    if (!scene) return;

    auto entity = scene->getEntityByUUID(uuid);
    if (!entity.valid()) return;

    auto cmd = std::make_unique<CreateEntityCommand>(newUuid, entity.name() + " (Copy)",
        std::optional<dodoe::Uuid>());
    m_ctx.commands().execute(std::move(cmd));
    m_ctx.events().entityCreated.emit(newUuid);
    m_ctx.selection().select(newUuid);
}

void HierarchyPanel::deleteEntity(dodoe::Uuid uuid)
{
    auto cmd = std::make_unique<DeleteEntityCommand>(uuid);
    m_ctx.commands().execute(std::move(cmd));
    m_ctx.events().entityDestroyed.emit(uuid);
    m_ctx.selection().clear();
}

void HierarchyPanel::renameEntity(QTreeWidgetItem* item, dodoe::Uuid)
{
    if (!item) return;
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    m_tree->editItem(item);
}

void HierarchyPanel::reparentEntity(dodoe::Uuid entity, dodoe::Uuid newParent)
{
    auto* scene = m_ctx.activeScene();
    if (!scene) return;

    auto ent = scene->getEntityByUUID(entity);
    if (!ent.valid()) return;

    dodoe::Uuid oldParent;
    if (ent.hasComponent<dodoe::HierarchyComponent>()) {
        oldParent = ent.getComponent<dodoe::HierarchyComponent>().parent_uuid;
    }

    auto cmd = std::make_unique<ReparentEntityCommand>(entity, oldParent, newParent);
    m_ctx.commands().execute(std::move(cmd));
    m_ctx.events().hierarchyChanged.emit(entity);
}

bool HierarchyPanel::eventFilter(QObject* obj, QObject* subject, QEvent* event)
{
    if (obj == m_tree && event->type() == QEvent::Drop) {
        auto* de = static_cast<QDropEvent*>(event);
        auto* sourceItem = m_tree->currentItem();
        auto* targetItem = m_tree->itemAt(m_tree->mapFromGlobal(QCursor::pos()));

        if (sourceItem && targetItem && sourceItem != targetItem) {
            auto srcUuid = dodoe::Uuid(sourceItem->data(0, kEntityUuidRole).toULongLong());
            auto dstUuid = dodoe::Uuid(targetItem->data(0, kEntityUuidRole).toULongLong());
            reparentEntity(srcUuid, dstUuid);
        }
    }
    return QWidget::eventFilter(obj, event);
}

} // namespace cakery
