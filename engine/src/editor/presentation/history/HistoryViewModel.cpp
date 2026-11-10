// do@Redlive

#include "dopch.h"

#include "HistoryViewModel.h"

namespace cakery {

HistoryViewModel::HistoryViewModel(EditorSession& session)
    : m_session(session)
{
    m_historySubscription = m_session.history().subscribe([this]() { refresh(); });
    refresh();
}

void HistoryViewModel::undo()
{
    m_session.undo();
}

void HistoryViewModel::redo()
{
    m_session.redo();
}

void HistoryViewModel::clear()
{
    m_session.history().clear();
}

void HistoryViewModel::refresh()
{
    const EditorHistory& history = m_session.history();
    HistorySnapshot next;
    next.canUndo = history.canUndo();
    next.canRedo = history.canRedo();
    for (const auto& entry : history.redoStack()) {
        next.redoLabels.push_back(entry->label());
    }
    for (const auto& entry : history.undoStack()) {
        next.undoLabels.push_back(entry->label());
    }
    m_snapshot = std::move(next);
    notifyChanged();
}

} // namespace cakery
