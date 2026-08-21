// do@Redlive

#pragma once

#include "Panel.h"
#include <QTableWidget>
#include <QTimer>

namespace cakery {

class MemoryPanel : public Panel {
    Q_OBJECT
public:
    explicit MemoryPanel(EditorContext& ctx, QWidget* parent = nullptr);

private slots:
    void refresh();

private:
    QTableWidget* m_table{nullptr};
    QTimer* m_timer{nullptr};
};

} // namespace cakery
