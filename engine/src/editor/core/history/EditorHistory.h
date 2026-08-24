// do@Redlive

#pragma once

#include "core/commands/EditorCommand.h"
#include "core/Signal.h"

#include <functional>
#include <memory>
#include <vector>

namespace cakery {

class EditorDocumentModel;

class EditorHistory {
public:
    EditorCommand* execute(std::unique_ptr<EditorCommand> command, EditorDocumentModel& model);
    bool undo(EditorDocumentModel& model);
    bool redo(EditorDocumentModel& model);
    void clear();

    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }
    std::size_t undoCount() const { return m_undoStack.size(); }
    std::size_t redoCount() const { return m_redoStack.size(); }

    void beginMerge() { m_merging = true; }
    void endMerge() { m_merging = false; m_lastMergeable = nullptr; }

    std::string undoLabel() const;
    std::string redoLabel() const;

    ScopedConnection subscribe(std::function<void()> onChange);

private:
    void emitChanged() const;

    std::vector<std::unique_ptr<EditorCommand>> m_undoStack;
    std::vector<std::unique_ptr<EditorCommand>> m_redoStack;
    bool m_merging = false;
    EditorCommand* m_lastMergeable = nullptr;
    Signal<> m_changed;
};

} // namespace cakery
