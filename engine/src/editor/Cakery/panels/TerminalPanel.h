// do@Redlive

#pragma once

#include "Panel.h"
#include <QListWidget>
#include <QLineEdit>

namespace cakery {

class TerminalPanel : public Panel {
    Q_OBJECT
public:
    explicit TerminalPanel(EditorContext& ctx, QWidget* parent = nullptr);

private:
    void submit(const QString& line);

    QListWidget* m_output = nullptr;
    QLineEdit*   m_input  = nullptr;
    QStringList  m_history;
    int          m_historyPos = 0;
};

} // namespace cakery
