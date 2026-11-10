// do@Redlive
#pragma once
#include "dopch.h"

#include "EditorCommand.h"
#include <vector>
#include <memory>
#include <string>

namespace cakery {

class CompositeCommand final : public EditorCommand {
public:
    void addCommand(Scope<EditorCommand> cmd) {
        m_commands.push_back(std::move(cmd));
    }

    bool empty() const { return m_commands.empty(); }
    Size_t count() const { return m_commands.size(); }

    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;

private:
    DynamicArray<Scope<EditorCommand>> m_commands;
};

} // namespace cakery
