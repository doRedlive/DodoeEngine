// do@Redlive

#pragma once

#include "Panel.h"
#include <QTreeWidget>

namespace cakery {

class ProjectPanel : public Panel {
    Q_OBJECT
public:
    explicit ProjectPanel(EditorContext& ctx, QWidget* parent = nullptr);

    void setBasePath(const QString& path);

private:
    QTreeWidget* m_tree = nullptr;
};

} // namespace cakery
