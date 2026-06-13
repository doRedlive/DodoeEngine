

#include "HierarchyWidget.h"
#include "services/EngineManager.h"

#include "runtime/function/world/entity.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/id_component.h"
#include "runtime/function/world/components/transform_component.h"

#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QLabel>
#include <QHBoxLayout>

namespace cakery {

static const int kEntityIdRole = Qt::UserRole + 1;

HierarchyWidget::HierarchyWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);
    layout->setSpacing(3);


    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(3);

    m_createBtn = new QPushButton(tr("Create"), this);
    m_createBtn->setToolTip(tr("Create GameObject"));
    toolbar->addWidget(m_createBtn);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search..."));
    toolbar->addWidget(m_searchEdit, 1);

    layout->addLayout(toolbar);


    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setDragEnabled(true);
    m_tree->setAcceptDrops(true);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    layout->addWidget(m_tree, 1);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &HierarchyWidget::onSearchChanged);
    connect(m_createBtn, &QPushButton::clicked, this, &HierarchyWidget::onCreateEntity);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &HierarchyWidget::onItemSelected);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &HierarchyWidget::onItemDoubleClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &HierarchyWidget::onContextMenu);


    connect(&EngineManager::getInstance(), &EngineManager::engineInitialized,
            this, &HierarchyWidget::refresh);
}

void HierarchyWidget::refresh()
{
    m_tree->clear();

    auto* scene = EngineManager::getInstance().getCurrentScene();
    if (!scene) return;

    auto entities = scene->getEntities();
    if (entities.empty()) return;



    std::unordered_map<entt::entity, QTreeWidgetItem*> itemMap;


    for (auto& entity : entities) {
        if (!entity.valid()) continue;

        auto* item = new QTreeWidgetItem();
        item->setText(0, QString::fromStdString(entity.name()));
        item->setData(0, kEntityIdRole, QVariant::fromValue(static_cast<quint64>(entity.handle())));
        itemMap[entity.handle()] = item;
    }


    for (auto& entity : entities) {
        if (!entity.valid()) continue;

        auto* item = itemMap[entity.handle()];
        if (!item) continue;

        bool hasParent = false;
        if (entity.hasComponent<dodoe::HierarchyComponent>()) {
            auto& hc = entity.getComponent<dodoe::HierarchyComponent>();
            if (hc.parent.valid()) {
                auto parentIt = itemMap.find(hc.parent.handle());
                if (parentIt != itemMap.end()) {

                    int rootIdx = m_tree->indexOfTopLevelItem(item);
                    if (rootIdx >= 0) {
                        m_tree->takeTopLevelItem(rootIdx);
                    }
                    parentIt->second->addChild(item);
                    hasParent = true;
                }
            }
        }

        if (!hasParent) {

            if (m_tree->indexOfTopLevelItem(item) < 0) {
                m_tree->addTopLevelItem(item);
            }
        }
    }


    if (!m_searchText.isEmpty()) {
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            auto* item = m_tree->topLevelItem(i);
            item->setHidden(!matchesSearch(item));
        }
    }


    m_tree->expandAll();
}

void HierarchyWidget::populateItem(QTreeWidgetItem* parentItem, dodoe::Entity entity)
{
    if (!entity.hasComponent<dodoe::HierarchyComponent>()) return;
    auto& hc = entity.getComponent<dodoe::HierarchyComponent>();

    for (auto& child : hc.children) {
        if (!child.valid()) continue;
        auto* childItem = new QTreeWidgetItem();
        childItem->setText(0, QString::fromStdString(child.name()));
        childItem->setData(0, kEntityIdRole, QVariant::fromValue(static_cast<quint64>(child.handle())));
        parentItem->addChild(childItem);
        populateItem(childItem, child);
    }
}

bool HierarchyWidget::matchesSearch(QTreeWidgetItem* item) const
{
    if (m_searchText.isEmpty()) return true;
    if (item->text(0).contains(m_searchText, Qt::CaseInsensitive)) return true;
    for (int i = 0; i < item->childCount(); ++i) {
        if (matchesSearch(item->child(i))) return true;
    }
    return false;
}

void HierarchyWidget::onSearchChanged(const QString& text)
{
    m_searchText = text;
    refresh();
}

void HierarchyWidget::onCreateEntity()
{
    auto* scene = EngineManager::getInstance().getCurrentScene();
    if (!scene) return;

    auto entity = scene->createEntity("New Entity");


    auto* selectedItem = m_tree->currentItem();
    if (selectedItem) {
        auto parentId = static_cast<entt::entity>(selectedItem->data(0, kEntityIdRole).value<quint64>());
        dodoe::Entity parentEntity(scene, parentId);
        if (parentEntity.valid() && parentEntity.handle() != entity.handle()) {

            if (!parentEntity.hasComponent<dodoe::HierarchyComponent>())
                parentEntity.addComponent<dodoe::HierarchyComponent>();


            if (!entity.hasComponent<dodoe::HierarchyComponent>())
                entity.addComponent<dodoe::HierarchyComponent>();

            entity.getComponent<dodoe::HierarchyComponent>().parent = parentEntity;
            parentEntity.getComponent<dodoe::HierarchyComponent>().children.push_back(entity);
        }
    }

    refresh();
}

void HierarchyWidget::onDeleteSelected()
{
    auto* item = m_tree->currentItem();
    if (!item) return;

    auto id = static_cast<entt::entity>(item->data(0, kEntityIdRole).value<quint64>());
    auto* scene = EngineManager::getInstance().getCurrentScene();
    if (!scene) return;

    dodoe::Entity entity(scene, id);
    if (entity.valid()) {

        if (entity.hasComponent<dodoe::HierarchyComponent>()) {
            auto& hc = entity.getComponent<dodoe::HierarchyComponent>();
            if (hc.parent.valid() && hc.parent.hasComponent<dodoe::HierarchyComponent>()) {
                auto& parentChildren = hc.parent.getComponent<dodoe::HierarchyComponent>().children;
                parentChildren.erase(std::remove(parentChildren.begin(), parentChildren.end(), entity),
                                     parentChildren.end());
            }

            for (auto& child : hc.children) {
                if (child.valid()) {
                    scene->destroyEntity(child);
                }
            }
        }
        scene->destroyEntity(entity);
    }

    emit entityDeselected();
    refresh();
}

void HierarchyWidget::onItemSelected()
{
    auto* item = m_tree->currentItem();
    if (!item) {
        emit entityDeselected();
        return;
    }

    auto id = static_cast<entt::entity>(item->data(0, kEntityIdRole).value<quint64>());
    auto* scene = EngineManager::getInstance().getCurrentScene();
    if (!scene) { emit entityDeselected(); return; }

    dodoe::Entity entity(scene, id);
    if (entity.valid()) {
        emit entitySelected(entity);
    } else {
        emit entityDeselected();
    }
}

void HierarchyWidget::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (!item) return;
    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename Entity"),
                                             tr("New name:"), QLineEdit::Normal,
                                             item->text(0), &ok);
    if (ok && !newName.isEmpty()) {
        auto id = static_cast<entt::entity>(item->data(0, kEntityIdRole).value<quint64>());
        auto* scene = EngineManager::getInstance().getCurrentScene();
        if (!scene) return;

        dodoe::Entity entity(scene, id);
        if (entity.valid() && entity.hasComponent<dodoe::IDComponent>()) {
            entity.getComponent<dodoe::IDComponent>().name = newName.toStdString();
            item->setText(0, newName);
        }
    }
}

void HierarchyWidget::onContextMenu(const QPoint& pos)
{
    auto* item = m_tree->itemAt(pos);
    QMenu menu(this);

    if (item) {
        m_tree->setCurrentItem(item);
        menu.addAction(tr("Rename"), this, [this, item]() { onItemDoubleClicked(item, 0); });
        menu.addAction(tr("Delete"), this, &HierarchyWidget::onDeleteSelected);
        menu.addSeparator();
    }

    menu.addAction(tr("Create Empty Entity"), this, &HierarchyWidget::onCreateEntity);
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

}
