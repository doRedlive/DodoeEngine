// do@Redlive

#include "CompositeCommand.h"

#include "core/document/EditorDocumentModel.h"

namespace cakery {

bool CompositeCommand::execute(EditorDocumentModel& model)
{
    std::vector<EditorCommand*> executed;
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

std::string CompositeCommand::label() const
{
    return "Composite (" + std::to_string(m_commands.size()) + " commands)";
}

} // namespace cakery
