// do@Redlive

#pragma once

#include "ICommand.h"
#include <vector>
#include <memory>
#include <string>

namespace cakery {

class CompositeCommand : public ICommand {
public:
    void addCommand(std::unique_ptr<ICommand> cmd) {
        m_commands.push_back(std::move(cmd));
    }

    bool empty() const { return m_commands.empty(); }
    size_t count() const { return m_commands.size(); }

    bool execute(EditorContext& ctx) override;
    void undo(EditorContext& ctx) override;
    std::string label() const override;

private:
    std::vector<std::unique_ptr<ICommand>> m_commands;
};

} // namespace cakery
