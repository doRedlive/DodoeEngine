// do@Redlive
#pragma once
#include "dopch.h"

#include "core/EditorSession.h"
#include "presentation/ViewModel.h"

namespace cakery {

struct HistorySnapshot {
    DynamicArray<String> redoLabels;
    DynamicArray<String> undoLabels;
    bool canUndo = false;
    bool canRedo = false;
};

class HistoryViewModel : public ViewModel {
public:
    explicit HistoryViewModel(EditorSession& session);

    const HistorySnapshot& snapshot() const { return m_snapshot; }

    void undo();
    void redo();
    void clear();
    void refresh();

private:
    EditorSession& m_session;
    HistorySnapshot m_snapshot;
    ScopedConnection m_historySubscription;
};

} // namespace cakery
