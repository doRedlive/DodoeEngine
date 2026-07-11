// do@Redlive

#include "CompositeCommand.h"
#include "framework/EditorContext.h"

namespace cakery {

bool CompositeCommand::execute(EditorContext& ctx)
{
    for (auto& cmd : m_commands) {
        if (cmd && !cmd->execute(ctx)) {
            return false;
        }
    }
    return true;
}

void CompositeCommand::undo(EditorContext& ctx)
{
    for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
        if (*it) {
            (*it)->undo(ctx);
        }
    }
}

std::string CompositeCommand::label() const
{
    return "Composite (" + std::to_string(m_commands.size()) + " commands)";
}

} // namespace cakery
