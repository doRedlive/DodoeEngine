// do@Redlive

#include "EditorHistory.h"

#include "core/document/EditorDocumentModel.h"

#include <utility>

namespace cakery {

EditorCommand* EditorHistory::execute(std::unique_ptr<EditorCommand> command, EditorDocumentModel& model) {
    if (!command) {
        return nullptr;
    }
    command->execute(model);

    if (m_merging && m_lastMergeable && m_lastMergeable->mergeWith(*command)) {
        return nullptr;
    }

    m_lastMergeable = command.get();
    m_undoStack.push_back(std::move(command));
    m_redoStack.clear();
    emitChanged();
    return m_undoStack.back().get();
}

bool EditorHistory::undo(EditorDocumentModel& model) {
    if (m_undoStack.empty()) {
        return false;
    }
    m_lastMergeable = nullptr;
    std::unique_ptr<EditorCommand> command = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    command->revert(model);
    m_redoStack.push_back(std::move(command));
    emitChanged();
    return true;
}

bool EditorHistory::redo(EditorDocumentModel& model) {
    if (m_redoStack.empty()) {
        return false;
    }
    m_lastMergeable = nullptr;
    std::unique_ptr<EditorCommand> command = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    command->execute(model);
    m_undoStack.push_back(std::move(command));
    emitChanged();
    return true;
}

void EditorHistory::clear() {
    m_undoStack.clear();
    m_redoStack.clear();
    m_lastMergeable = nullptr;
    emitChanged();
}

std::string EditorHistory::undoLabel() const {
    if (m_undoStack.empty()) {
        return {};
    }
    return m_undoStack.back()->label();
}

std::string EditorHistory::redoLabel() const {
    if (m_redoStack.empty()) {
        return {};
    }
    return m_redoStack.back()->label();
}

void EditorHistory::subscribe(std::function<void()> onChange) {
    m_observers.push_back(std::move(onChange));
}

void EditorHistory::emitChanged() const {
    for (const auto& observer : m_observers) {
        observer();
    }
}

} // namespace cakery
