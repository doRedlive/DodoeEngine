// do@Redlive

#pragma once

#include "Panel.h"
#include <QTreeWidget>

namespace cakery {

class HierarchyPanel : public Panel {
    Q_OBJECT
public:
    explicit HierarchyPanel(EditorContext& ctx, QWidget* parent = nullptr);

    void refresh();

private slots:
    void onItemSelected();

private:
    QTreeWidget* m_tree = nullptr;
};

} // namespace cakery
