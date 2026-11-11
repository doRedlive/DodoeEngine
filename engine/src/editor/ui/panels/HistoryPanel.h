// do@Redlive
#pragma once
#include "dopch.h"

#include <QWidget>

#include "core/Signal.h"

class QListWidget;
class QLineEdit;
class QToolButton;

namespace cakery {

class EditorWorkspaceContext;
class HistoryViewModel;

class HistoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);
    ~HistoryPanel() override;

private:
    void onViewModelChanged();
    void refresh();
    void undo();
    void redo();
    void clear();

    EditorWorkspaceContext& m_context;
    Scope<HistoryViewModel> m_viewModel;
    ScopedConnection m_viewModelConnection;
    QListWidget* m_list = nullptr;
    QLineEdit* m_search = nullptr;
    QToolButton* m_undo = nullptr;
    QToolButton* m_redo = nullptr;
};

} // namespace cakery
