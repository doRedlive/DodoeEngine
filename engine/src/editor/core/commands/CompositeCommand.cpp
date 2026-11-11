// do@Redlive

#include "dopch.h"

#include "CompositeCommand.h"

#include "core/document/EditorDocumentModel.h"

namespace cakery {

bool CompositeCommand::execute(EditorDocumentModel& model)
{
    DynamicArray<EditorCommand*> executed;
    for (auto& cmd : m_commands) {
        if (!cmd) {
            continue;
        }
        if (!cmd->execute(model)) {
            for (auto it = executed.rbegin(); it != executed.rend(); ++it) {
                (*it)->revert(model);
            }
            return false;
        }
        executed.push_back(cmd.get());
    }
    return true;
}

void CompositeCommand::revert(EditorDocumentModel& model)
{
    for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
        if (*it) {
            (*it)->revert(model);
        }
    }
}

String CompositeCommand::label() const
{
    return "Composite (" + ToString(m_commands.size()) + " commands)";
}

bool CompositeCommand::canMergeWith(const EditorCommand& next) const
{
    const auto* other = dynamic_cast<const CompositeCommand*>(&next);
    if (!other || other->m_commands.size() != m_commands.size()) return false;
    for (Size_t i = 0; i < m_commands.size(); ++i) {
        if (!m_commands[i] || !other->m_commands[i]
            || !m_commands[i]->canMergeWith(*other->m_commands[i])) return false;
    }
    return true;
}

bool CompositeCommand::mergeWith(const EditorCommand& next)
{
    if (!canMergeWith(next)) return false;
    const auto& other = static_cast<const CompositeCommand&>(next);
    for (Size_t i = 0; i < m_commands.size(); ++i) m_commands[i]->mergeWith(*other.m_commands[i]);
    return true;
}

bool CompositeCommand::isObsolete() const
{
    for (const auto& command : m_commands) {
        if (command && !command->isObsolete()) return false;
    }
    return true;
}

} // namespace cakery
