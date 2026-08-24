// do@Redlive

#pragma once

#include <QWidget>

#include "core/Signal.h"

class QListWidget;
class QListWidgetItem;

namespace cakery {

class EditorWorkspaceContext;

class HistoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);

private:
    void refresh();
    void onItemClicked(QListWidgetItem* item);

    EditorWorkspaceContext& m_context;
    QListWidget* m_list = nullptr;
    ScopedConnection m_historySubscription;
};

} // namespace cakery
