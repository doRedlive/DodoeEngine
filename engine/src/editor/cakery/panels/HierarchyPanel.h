// do@Redlive

#pragma once

#include "Panel.h"
#include "runtime/core/utils/uuid.h"

#include <QTreeWidget>
#include <QLineEdit>
#include <QMenu>

namespace dodoe {
class Scene;
}

namespace cakery {

class HierarchyPanel : public Panel {
    Q_OBJECT
public:
    explicit HierarchyPanel(EditorContext& ctx, QWidget* parent = nullptr);

    void refresh();

private slots:
    void onItemSelected();
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onItemChanged(QTreeWidgetItem* item, int column);
    void onCustomContextMenu(const QPoint& pos);
    void onSearchTextChanged(const QString& text);

private:
    void populateTree();
    void addEntityItem(QTreeWidgetItem* parent, dodoe::UUID uuid, dodoe::Scene* scene);
    void createEmptyEntity(dodoe::UUID parentUuid = dodoe::UUID());
    void duplicateEntity(dodoe::UUID uuid);
    void deleteEntity(dodoe::UUID uuid);
    void renameEntity(QTreeWidgetItem* item, dodoe::UUID uuid);
    void reparentEntity(dodoe::UUID entity, dodoe::UUID newParent);

    bool eventFilter(QObject* obj, QEvent* event) override;

    QLineEdit* m_searchBox = nullptr;
    QTreeWidget* m_tree = nullptr;
    QString m_filterText;
};

} // namespace cakery
