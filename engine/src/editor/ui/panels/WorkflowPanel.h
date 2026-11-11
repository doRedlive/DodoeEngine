// do@Redlive
#pragma once
#include "dopch.h"

#include <QWidget>

#include "core/Signal.h"


class QComboBox;
class QPlainTextEdit;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace cakery {

class EditorWorkspaceContext;
class WorkflowGraphView;
class WorkflowViewModel;

class WorkflowPanel : public QWidget {
    Q_OBJECT
public:
    explicit WorkflowPanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);
    ~WorkflowPanel() override;

private:
    void onViewModelChanged();
    void refresh();
    void showDetails(QTreeWidgetItem* current);
    void selectRecord(UInt64 sequence);
    void captureTemplate();
    void runTemplate();
    void clearHistory();
    void showContextMenu(const QPoint& pos);
    void rerunRecord(const Json& record, const Json* argsOverride);
    String authorFilter() const;

    EditorWorkspaceContext& m_context;
    Scope<WorkflowViewModel> m_viewModel;
    ScopedConnection m_viewModelConnection;
    WorkflowGraphView* m_graph = nullptr;
    QTreeWidget* m_list = nullptr;
    QComboBox* m_filter = nullptr;
    QComboBox* m_author = nullptr;
    QComboBox* m_template = nullptr;
    QToolButton* m_run = nullptr;
    QPlainTextEdit* m_details = nullptr;
};

} // namespace cakery
