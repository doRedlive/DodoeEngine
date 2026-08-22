// do@Redlive

#pragma once

#include "EditorCommand.h"
#include <vector>
#include <memory>
#include <string>

namespace cakery {

class CompositeCommand final : public EditorCommand {
public:
    void addCommand(std::unique_ptr<EditorCommand> cmd) {
        m_commands.push_back(std::move(cmd));
    }

    bool empty() const { return m_commands.empty(); }
    size_t count() const { return m_commands.size(); }

    void execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

private:
    std::vector<std::unique_ptr<EditorCommand>> m_commands;
};

} // namespace cakery
