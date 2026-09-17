// do@Redlive

#include "HierarchyPanel.h"

#include "cakery/ui/EditorWorkspaceContext.h"

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

std::uint64_t ItemUuid(QTreeWidgetItem* item) {
    return item ? item->data(0, Qt::UserRole).toString().toULongLong() : 0;
}

class HierarchyTree final : public QTreeWidget {
public:
    using ReparentCallback = std::function<void(std::uint64_t, std::uint64_t)>;

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
        const std::uint64_t child = ItemUuid(source);
        const std::uint64_t parent = ItemUuid(target);
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
    : QWidget(parent), m_context(context)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Search hierarchy..."));
    m_filter->setClearButtonEnabled(true);
    layout->addWidget(m_filter);

    m_tree = new HierarchyTree(
        [this](std::uint64_t uuid, std::uint64_t parent) { onReparentEntity(uuid, parent); }, this);
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

    m_documentSubscription = m_context.session().documentModel().subscribe([this]() { refresh(); });
    m_selectionSubscription = m_context.session().selection().subscribe([this]() { refreshSelection(); });
    refresh();
}

void HierarchyPanel::refresh()
{
    const auto& entities = m_context.session().documentModel().entities();
    const auto& selection = m_context.session().selection();

    QSignalBlocker blocker(m_tree);
    m_tree->clear();
    std::unordered_map<std::uint64_t, std::vector<const EditorEntity*>> children;
    std::vector<const EditorEntity*> roots;
    for (const auto& entity : entities) {
        if (entity.parent != 0 && m_context.session().documentModel().findEntity(entity.parent)) {
            children[entity.parent].push_back(&entity);
        } else {
            roots.push_back(&entity);
        }
    }

    const auto addItem = [&](auto&& self, const EditorEntity& entity, QTreeWidgetItem* parent) -> void {
        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
        item->setText(0, QString::fromStdString(entity.name));
        item->setData(0, Qt::UserRole, QString::number(entity.uuid));
        item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
        item->setSelected(selection.isSelected(entity.uuid));
        const auto childIt = children.find(entity.uuid);
        if (childIt != children.end() && !childIt->second.empty()) {
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }
        for (const EditorEntity* child : children[entity.uuid]) {
            self(self, *child, item);
        }
    };
    for (const EditorEntity* root : roots) {
        addItem(addItem, *root, nullptr);
    }
    blocker.unblock();

    for (std::uint64_t uuid : selection.selectedAll()) {
        if (!m_context.session().documentModel().findEntity(uuid)) {
            m_context.session().selection().remove(uuid);
        }
    }
    applyFilter();
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
    const auto& selection = m_context.session().selection();
    QSignalBlocker blocker(m_tree);
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
        (*it)->setSelected(selection.isSelected(ItemUuid(*it)));
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
    std::vector<std::uint64_t> selected;
    selected.reserve(static_cast<std::size_t>(items.size()));
    for (QTreeWidgetItem* item : items) {
        const std::uint64_t uuid = ItemUuid(item);
        if (uuid != 0) {
            selected.push_back(uuid);
        }
    }
    m_context.session().selection().selectMany(std::move(selected));
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
    const std::uint64_t uuid = ItemUuid(item);
    const QString name = item->text(0);
    QTimer::singleShot(0, this, [this, uuid, name]() {
        m_context.session().renameEntity(uuid, name.toStdString());
    });
}

void HierarchyPanel::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (item) {
        m_tree->setCurrentItem(item);
        item->setSelected(true);
    }
    QMenu menu(this);
    QAction* create = menu.addAction(tr("Create GameObject"));
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
    if (m_context.session().hasEntityClipboard()) {
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
    m_context.session().createEntity(tr("New GameObject").toStdString());
}

void HierarchyPanel::onCreateChildEntity(std::uint64_t parentUuid)
{
    const std::uint64_t childUuid = m_context.session().createEntity(tr("New GameObject").toStdString());
    if (childUuid != 0 && parentUuid != 0) {
        m_context.session().reparentEntity(childUuid, parentUuid);
    }
}

void HierarchyPanel::onDeleteEntity()
{
    QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) {
        return;
    }
    std::vector<std::uint64_t> uuids;
    uuids.reserve(static_cast<std::size_t>(items.size()));
    for (QTreeWidgetItem* item : items) {
        const std::uint64_t uuid = ItemUuid(item);
        if (uuid != 0) uuids.push_back(uuid);
    }
    m_context.session().deleteEntities(uuids);
}

void HierarchyPanel::onMoveToRoot()
{
    for (QTreeWidgetItem* item : m_tree->selectedItems()) {
        m_context.session().reparentEntity(ItemUuid(item), 0);
    }
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
    std::vector<std::uint64_t> uuids;
    for (QTreeWidgetItem* item : m_tree->selectedItems()) {
        const std::uint64_t uuid = ItemUuid(item);
        if (uuid != 0) uuids.push_back(uuid);
    }
    m_context.session().copyEntities(uuids);
}

void HierarchyPanel::onCutEntities()
{
    std::vector<std::uint64_t> uuids;
    for (QTreeWidgetItem* item : m_tree->selectedItems()) {
        const std::uint64_t uuid = ItemUuid(item);
        if (uuid != 0) uuids.push_back(uuid);
    }
    m_context.session().cutEntities(uuids);
}

void HierarchyPanel::onPasteEntities()
{
    m_context.session().pasteEntities();
}

void HierarchyPanel::onDuplicateEntities()
{
    std::vector<std::uint64_t> uuids;
    for (QTreeWidgetItem* item : m_tree->selectedItems()) {
        const std::uint64_t uuid = ItemUuid(item);
        if (uuid != 0) uuids.push_back(uuid);
    }
    m_context.session().duplicateEntities(uuids);
}

void HierarchyPanel::onSaveAsPrefab()
{
    QTreeWidgetItem* current = m_tree->currentItem();
    const std::uint64_t uuid = ItemUuid(current);
    if (uuid == 0) {
        return;
    }
    const std::filesystem::path assetRoot = m_context.session().assetRoot();
    QString suggested;
    if (!assetRoot.empty()) {
        suggested = QDir(QString::fromStdString(assetRoot.string())).filePath(
            current->text(0) + QStringLiteral(".prefab"));
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save As Prefab"), suggested, tr("Prefab (*.prefab)"));
    if (path.isEmpty()) {
        return;
    }
    QString target = path;
    if (!assetRoot.empty()) {
        std::error_code ec;
        const std::filesystem::path relative = std::filesystem::relative(
            std::filesystem::path(path.toStdString()), assetRoot, ec);
        if (!ec && !relative.empty() && !relative.generic_string().starts_with("..")) {
            target = QString::fromStdString(relative.generic_string());
        }
    }
    const QString payload = QString::number(static_cast<qulonglong>(uuid)) +
                            QStringLiteral(",") + target;
    if (!m_context.session().execute({"prefab.export", payload.toStdString()})) {
        QMessageBox::warning(this, tr("Save As Prefab"),
                             tr("Could not export the selected GameObject as a prefab."));
    }
}

void HierarchyPanel::onReparentEntity(std::uint64_t uuid, std::uint64_t newParent)
{
    m_context.session().reparentEntity(uuid, newParent);
}

} // namespace cakery
