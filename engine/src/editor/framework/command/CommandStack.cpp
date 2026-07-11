// do@Redlive

#include "CommandStack.h"
#include "framework/EditorContext.h"

namespace cakery {

void CommandStack::execute(std::unique_ptr<ICommand> cmd)
{
    if (!cmd) return;

    if (!cmd->execute(m_ctx)) {
        return;
    }

    if (m_merging && m_lastMergeable && m_lastMergeable->mergeWith(*cmd)) {
        return;
    }

    m_lastMergeable = cmd.get();
    m_undo.push_back(std::move(cmd));
    m_redo.clear();
    changed.fire();
}

void CommandStack::undo()
{
    if (m_undo.empty()) return;

    auto cmd = std::move(m_undo.back());
    m_undo.pop_back();
    cmd->undo(m_ctx);
    m_redo.push_back(std::move(cmd));
    changed.fire();
}

void CommandStack::redo()
{
    if (m_redo.empty()) return;

    auto cmd = std::move(m_redo.back());
    m_redo.pop_back();
    cmd->redo(m_ctx);
    m_undo.push_back(std::move(cmd));
    changed.fire();
}

void CommandStack::clear()
{
    m_undo.clear();
    m_redo.clear();
    m_lastMergeable = nullptr;
    changed.fire();
}

std::string CommandStack::undoLabel() const
{
    if (m_undo.empty()) return {};
    return m_undo.back()->label();
}

std::string CommandStack::redoLabel() const
{
    if (m_redo.empty()) return {};
    return m_redo.back()->label();
}

} // namespace cakery
