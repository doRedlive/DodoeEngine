// do@Redlive

#pragma once

#include <QWidget>

#include "core/Signal.h"

#include <nlohmann/json.hpp>

class QComboBox;
class QPlainTextEdit;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace cakery {

class EditorWorkspaceContext;
class WorkflowGraphView;

class WorkflowPanel : public QWidget {
    Q_OBJECT
public:
    explicit WorkflowPanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);

private:
    void refresh();
    void showDetails(QTreeWidgetItem* current);
    void selectRecord(std::uint64_t sequence);
    void refreshTemplates();
    void captureTemplate();
    void runTemplate();
    void clearHistory();
    void showContextMenu(const QPoint& pos);
    void rerunRecord(const nlohmann::json& record, const nlohmann::json* argsOverride);
    std::string authorFilter() const;

    EditorWorkspaceContext& m_context;
    WorkflowGraphView* m_graph = nullptr;
    QTreeWidget* m_list = nullptr;
    QComboBox* m_filter = nullptr;
    QComboBox* m_author = nullptr;
    QComboBox* m_template = nullptr;
    QToolButton* m_run = nullptr;
    QPlainTextEdit* m_details = nullptr;
    ScopedConnection m_recorderSubscription;
};

} // namespace cakery
