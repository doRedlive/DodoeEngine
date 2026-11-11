// do@Redlive
#pragma once
#include "dopch.h"

#include "core/commands/EditorCommand.h"
#include "core/Signal.h"

#include <functional>
#include <memory>
#include <vector>

namespace cakery {

class EditorDocumentModel;

class EditorHistory {
public:
    EditorCommand* execute(Scope<EditorCommand> command, EditorDocumentModel& model);
    bool undo(EditorDocumentModel& model);
    bool redo(EditorDocumentModel& model);
    void clear();

    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }
    Size_t undoCount() const { return m_undoStack.size(); }
    Size_t redoCount() const { return m_redoStack.size(); }
    const DynamicArray<Scope<EditorCommand>>& undoStack() const { return m_undoStack; }
    const DynamicArray<Scope<EditorCommand>>& redoStack() const { return m_redoStack; }

    void beginMerge() { m_merging = true; m_mergeGroup.clear(); }
    void endMerge();

    String undoLabel() const;
    String redoLabel() const;

    ScopedConnection subscribe(std::function<void()> onChange);

private:
    void emitChanged() const;

    DynamicArray<Scope<EditorCommand>> m_undoStack;
    DynamicArray<Scope<EditorCommand>> m_redoStack;
    bool m_merging = false;
    DynamicArray<EditorCommand*> m_mergeGroup;
    Signal<> m_changed;
};

} // namespace cakery
