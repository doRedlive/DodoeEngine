// do@Redlive

#pragma once

#include "framework/command/ICommand.h"
#include "runtime/core/utils/uuid.h"
#include <string>

namespace cakery {

class RenameEntityCommand : public ICommand {
public:
    RenameEntityCommand(dodoe::Uuid entity, std::string oldName, std::string newName);

    bool execute(EditorContext& ctx) override;
    void undo(EditorContext& ctx) override;
    std::string label() const override;

private:
    dodoe::Uuid m_entity;
    std::string m_oldName;
    std::string m_newName;
};

} // namespace cakery
