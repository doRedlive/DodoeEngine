// do@Redlive

#pragma once

#include <QWidget>

#include "core/Signal.h"

class QListWidget;
class QLineEdit;
class QToolButton;

namespace cakery {

class EditorWorkspaceContext;

class HistoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);

private:
    void refresh();
    void undo();
    void redo();
    void clear();

    EditorWorkspaceContext& m_context;
    QListWidget* m_list = nullptr;
    QLineEdit* m_search = nullptr;
    QToolButton* m_undo = nullptr;
    QToolButton* m_redo = nullptr;
    ScopedConnection m_historySubscription;
};

} // namespace cakery
