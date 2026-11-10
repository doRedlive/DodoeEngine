// do@Redlive

#include "dopch.h"

#include "HierarchyPanel.h"

#include "presentation/hierarchy/HierarchyViewModel.h"
#include "ui/common/QtStringInterop.h"

#include <QAbstractItemView>
#include <QDir>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QScrollBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <filesystem>
#include <functional>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cakery {

namespace {

void DrawTreeBranchIndicator(const QTreeWidget* tree, QPainter* painter, const QRect& rect,
                             const QModelIndex& index)
{
    painter->save();
    painter->fillRect(rect, tree->palette().color(QPalette::Base));
    if (!tree->model()->hasChildren(index)) {
        painter->restore();
        return;
    }

    const int centerX = rect.right() - tree->indentation() / 2;
    const int centerY = rect.center().y();
    QPainterPath path;
    if (tree->isExpanded(index)) {
        path.moveTo(centerX - 4, centerY - 2);
        path.lineTo(centerX, centerY + 2);
        path.lineTo(centerX + 4, centerY - 2);
    } else {
        path.moveTo(centerX - 2, centerY - 4);
        path.lineTo(centerX + 2, centerY);
        path.lineTo(centerX - 2, centerY + 4);
    }
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(tree->palette().color(QPalette::Text), 1.5,
                         Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->drawPath(path);
    painter->restore();
}

UInt64 ItemUuid(QTreeWidgetItem* item) {
    return item ? item->data(0, Qt::UserRole).toString().toULongLong() : 0;
}

class HierarchyTree final : public QTreeWidget {
public:
    using ReparentCallback = std::function<void(UInt64, UInt64)>;

    explicit HierarchyTree(ReparentCallback callback, QWidget* parent = nullptr)
        : QTreeWidget(parent), m_reparentCallback(std::move(callback))
    {
    }

protected:
    void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override
    {
        DrawTreeBranchIndicator(this, painter, rect, index);
    }

    void dropEvent(QDropEvent* event) override
    {
        const QList<QTreeWidgetItem*> selected = selectedItems();
        QTreeWidgetItem* source = selected.isEmpty() ? nullptr : selected.first();
        QTreeWidgetItem* target = itemAt(event->position().toPoint());
        if (!source || !target || source == target) {
            event->ignore();
            return;
        }
        const UInt64 child = ItemUuid(source);
        const UInt64 parent = ItemUuid(target);
        if (child == 0 || parent == 0) {
            event->ignore();
            return;
        }
        event->setDropAction(Qt::MoveAction);
        event->accept();
        if (m_reparentCallback) {
            const ReparentCallback callback = m_reparentCallback;
            QTimer::singleShot(0, this, [callback, child, parent]() {
                callback(child, parent);
            });
        }
    }

private:
    ReparentCallback m_reparentCallback;
};

} // namespace

HierarchyPanel::HierarchyPanel(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent),
      m_viewModel(create_scope<HierarchyViewModel>(
          context.documents(), context.selection(), context.assets()))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Search hierarchy..."));
    m_filter->setClearButtonEnabled(true);
    layout->addWidget(m_filter);

    m_tree = new HierarchyTree(
        [this](UInt64 uuid, UInt64 parent) { onReparentEntity(uuid, parent); }, this);
    m_tree->setHeaderHidden(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setDragEnabled(true);
    m_tree->setAcceptDrops(true);
    m_tree->setDropIndicatorShown(true);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree->setRootIsDecorated(true);
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &HierarchyPanel::onTreeSelectionChanged);
    connect(m_tree, &QTreeWidget::itemChanged, this, &HierarchyPanel::onItemEdited);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &HierarchyPanel::onContextMenu);
    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString&) { applyFilter(); });

    const auto addShortcut = [this](const QKeySequence& key, auto&& slot) {
        auto* shortcut = new QShortcut(key, m_tree);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, slot);
    };
    addShortcut(Qt::Key_Delete, &HierarchyPanel::onDeleteEntity);
    addShortcut(Qt::Key_F2, &HierarchyPanel::onRenameSelected);
    addShortcut(QKeySequence::Copy, &HierarchyPanel::onCopyEntities);
    addShortcut(QKeySequence::Cut, &HierarchyPanel::onCutEntities);
    addShortcut(QKeySequence::Paste, &HierarchyPanel::onPasteEntities);
    addShortcut(Qt::CTRL | Qt::Key_D, &HierarchyPanel::onDuplicateEntities);
    addShortcut(QKeySequence::SelectAll, &HierarchyPanel::onSelectAll);

    m_viewModelSubscription = ScopedConnection(
        m_viewModel->changed, m_viewModel->changed.connect([this]() { refresh(); }));
    m_selectionSubscription = ScopedConnection(
        m_viewModel->selectionChanged, m_viewModel->selectionChanged.connect([this]() {
            refreshSelection();
        }));
    refresh();
}

HierarchyPanel::~HierarchyPanel() = default;

void HierarchyPanel::refresh()
{
    UnorderedSet<UInt64> expanded;
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
        if ((*it)->isExpanded()) {
            expanded.insert(ItemUuid(*it));
        }
    }
    const int scrollPosition = m_tree->verticalScrollBar()->value();

    QSignalBlocker blocker(m_tree);
    m_tree->clear();

    const auto addItem = [&](auto&& self, const HierarchyNode& node, QTreeWidgetItem* parent) -> void {
        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
        item->setText(0, ToQString(node.name));
        item->setData(0, Qt::UserRole, QString::number(node.uuid));
        item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
        item->setSelected(m_viewModel->isSelected(node.uuid));
        if (expanded.contains(node.uuid)) {
            item->setExpanded(true);
        }
        if (!node.children.empty()) {
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }
        for (const HierarchyNode& child : node.children) {
            self(self, child, item);
        }
    };
    for (const HierarchyNode& root : m_viewModel->snapshot().roots) {
        addItem(addItem, root, nullptr);
    }
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
        if ((*it)->isSelected()) {
            m_tree->setCurrentItem(*it);
            break;
        }
    }
    blocker.unblock();

    applyFilter();
    m_tree->verticalScrollBar()->setValue(scrollPosition);
}

void HierarchyPanel::applyFilter()
{
    const QString needle = m_filter ? m_filter->text().trimmed() : QString();
    QSignalBlocker blocker(m_tree);
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        filterItem(m_tree->topLevelItem(i), needle);
    }
}

bool HierarchyPanel::filterItem(QTreeWidgetItem* item, const QString& needle)
{
    if (!item) {
        return false;
    }
    bool childVisible = false;
    for (int i = 0; i < item->childCount(); ++i) {
        childVisible = filterItem(item->child(i), needle) || childVisible;
    }
    const bool visible = needle.isEmpty() ||
        item->text(0).contains(needle, Qt::CaseInsensitive) || childVisible;
    item->setHidden(!visible);
    if (childVisible && !needle.isEmpty()) {
        item->setExpanded(true);
    }
    return visible;
}

void HierarchyPanel::refreshSelection()
{
    QSignalBlocker blocker(m_tree);
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
        (*it)->setSelected(m_viewModel->isSelected(ItemUuid(*it)));
    }
    blocker.unblock();
}

void HierarchyPanel::onTreeSelectionChanged()
{
    QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) {
        m_viewModel->clearSelection();
        return;
    }
    DynamicArray<UInt64> selected;
    selected.reserve(static_cast<Size_t>(items.size()));
    for (QTreeWidgetItem* item : items) {
        const UInt64 uuid = ItemUuid(item);
        if (uuid != 0) {
            selected.push_back(uuid);
        }
    }
    m_viewModel->select(selected);
}

void HierarchyPanel::onSelectAll()
{
    QSignalBlocker blocker(m_tree);
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
        if (!(*it)->isHidden()) {
            (*it)->setSelected(true);
        }
    }
    blocker.unblock();
    onTreeSelectionChanged();
}

void HierarchyPanel::onItemEdited(QTreeWidgetItem* item, int column)
{
    if (column != 0 || !item) {
        return;
    }
    const UInt64 uuid = ItemUuid(item);
    const QString name = item->text(0);
    QTimer::singleShot(0, this, [this, uuid, name]() {
        m_viewModel->renameEntity(uuid, ToDodoeString(name));
    });
}

void HierarchyPanel::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (item) {
        if (!item->isSelected()) {
            m_tree->clearSelection();
            m_tree->setCurrentItem(item);
            item->setSelected(true);
        }
    } else {
        m_tree->clearSelection();
        m_tree->setCurrentItem(nullptr);
    }
    QMenu menu(this);
    QAction* create = menu.addAction(tr("Create GameObject"));
    QAction* newTilemap = menu.addAction(tr("New Tilemap..."));
    QAction* createChild = nullptr;
    QAction* cut = nullptr;
    QAction* copy = nullptr;
    QAction* paste = nullptr;
    QAction* duplicate = nullptr;
    QAction* saveAsPrefab = nullptr;
    QAction* rename = nullptr;
    QAction* remove = nullptr;
    QAction* moveToRoot = nullptr;
    if (item) {
        menu.addSeparator();
        createChild = menu.addAction(tr("Create Child GameObject"));
        menu.addSeparator();
        cut = menu.addAction(tr("Cut"));
        copy = menu.addAction(tr("Copy"));
        duplicate = menu.addAction(tr("Duplicate"));
        menu.addSeparator();
        saveAsPrefab = menu.addAction(tr("Save As Prefab..."));
        rename = menu.addAction(tr("Rename"));
        moveToRoot = menu.addAction(tr("Move to Root"));
        remove = menu.addAction(tr("Delete"));
    }
    if (m_viewModel->hasClipboard()) {
        if (!item) {
            menu.addSeparator();
        }
        paste = menu.addAction(tr("Paste"));
    }

    QAction* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (!chosen) {
        return;
    }
    if (chosen == create) {
        onCreateEntity();
    } else if (chosen == newTilemap) {
        emit newTilemapRequested();
    } else if (chosen == createChild) {
        onCreateChildEntity(ItemUuid(item));
    } else if (chosen == cut) {
        onCutEntities();
    } else if (chosen == copy) {
        onCopyEntities();
    } else if (chosen == paste) {
        onPasteEntities();
    } else if (chosen == duplicate) {
        onDuplicateEntities();
    } else if (chosen == saveAsPrefab) {
        onSaveAsPrefab();
    } else if (chosen == rename) {
        onRenameSelected();
    } else if (chosen == moveToRoot) {
        onMoveToRoot();
    } else if (chosen == remove) {
        onDeleteEntity();
    }
}

void HierarchyPanel::onCreateEntity()
{
    m_viewModel->createEntity(ToDodoeString(tr("New GameObject")));
}

void HierarchyPanel::onCreateChildEntity(UInt64 parentUuid)
{
    m_viewModel->createChildEntity(ToDodoeString(tr("New GameObject")), parentUuid);
}

void HierarchyPanel::onDeleteEntity()
{
    QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) {
        return;
    }
    DynamicArray<UInt64> uuids;
    uuids.reserve(static_cast<Size_t>(items.size()));
    for (QTreeWidgetItem* item : items) {
        const UInt64 uuid = ItemUuid(item);
        if (uuid != 0) uuids.push_back(uuid);
    }
    m_viewModel->deleteEntities(uuids);
}

void HierarchyPanel::onMoveToRoot()
{
    DynamicArray<UInt64> uuids;
    for (QTreeWidgetItem* item : m_tree->selectedItems()) {
        const UInt64 uuid = ItemUuid(item);
        if (uuid != 0) uuids.push_back(uuid);
    }
    m_viewModel->moveToRoot(uuids);
}

void HierarchyPanel::onRenameSelected()
{
    QTreeWidgetItem* current = m_tree->currentItem();
    if (current) {
        m_tree->editItem(current, 0);
    }
}

void HierarchyPanel::onCopyEntities()
{
    DynamicArray<UInt64> uuids;
    for (QTreeWidgetItem* item : m_tree->selectedItems()) {
        const UInt64 uuid = ItemUuid(item);
        if (uuid != 0) uuids.push_back(uuid);
    }
    m_viewModel->copyEntities(uuids);
}

void HierarchyPanel::onCutEntities()
{
    DynamicArray<UInt64> uuids;
    for (QTreeWidgetItem* item : m_tree->selectedItems()) {
        const UInt64 uuid = ItemUuid(item);
        if (uuid != 0) uuids.push_back(uuid);
    }
    m_viewModel->cutEntities(uuids);
}

void HierarchyPanel::onPasteEntities()
{
    m_viewModel->pasteEntities();
}

void HierarchyPanel::onDuplicateEntities()
{
    DynamicArray<UInt64> uuids;
    for (QTreeWidgetItem* item : m_tree->selectedItems()) {
        const UInt64 uuid = ItemUuid(item);
        if (uuid != 0) uuids.push_back(uuid);
    }
    m_viewModel->duplicateEntities(uuids);
}

void HierarchyPanel::onSaveAsPrefab()
{
    QTreeWidgetItem* current = m_tree->currentItem();
    const UInt64 uuid = ItemUuid(current);
    if (uuid == 0) {
        return;
    }
    const FsPath assetRoot = m_viewModel->assetRoot();
    QString suggested;
    if (!assetRoot.empty()) {
        suggested = QDir(ToQString(assetRoot.string())).filePath(
            current->text(0) + QStringLiteral(".prefab"));
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save As Prefab"), suggested, tr("Prefab (*.prefab)"));
    if (path.isEmpty()) {
        return;
    }
    if (!m_viewModel->exportPrefab(uuid, ToDodoeString(path))) {
        QMessageBox::warning(this, tr("Save As Prefab"),
                             tr("Could not export the selected GameObject as a prefab."));
    }
}

void HierarchyPanel::onReparentEntity(UInt64 uuid, UInt64 newParent)
{
    m_viewModel->reparentEntity(uuid, newParent);
}

} // namespace cakery
