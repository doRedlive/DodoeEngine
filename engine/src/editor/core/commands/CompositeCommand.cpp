// do@Redlive

#include "CompositeCommand.h"

#include "core/document/EditorDocumentModel.h"

namespace cakery {

void CompositeCommand::execute(EditorDocumentModel& model)
{
    for (auto& cmd : m_commands) {
        if (cmd) {
            cmd->execute(model);
        }
    }
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
