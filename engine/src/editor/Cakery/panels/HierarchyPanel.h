// do@Redlive

#pragma once

#include "Panel.h"
#include "runtime/core/utils/uuid.h"

#include <QTreeWidget>
#include <QLineEdit>
#include <QMenu>

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
    void addEntityItem(QTreeWidgetItem* parent, dodoe::Uuid uuid, dodoe::Scene* scene);
    void createEmptyEntity(dodoe::Uuid parentUuid = dodoe::Uuid());
    void duplicateEntity(dodoe::Uuid uuid);
    void deleteEntity(dodoe::Uuid uuid);
    void renameEntity(QTreeWidgetItem* item, dodoe::Uuid uuid);
    void reparentEntity(dodoe::Uuid entity, dodoe::Uuid newParent);

    bool eventFilter(QObject* obj, QEvent* event) override;

    QLineEdit* m_searchBox = nullptr;
    QTreeWidget* m_tree = nullptr;
    QString m_filterText;
};

} // namespace cakery
