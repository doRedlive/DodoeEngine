// do@Redlive

#pragma once

#include "framework/command/ICommand.h"
#include "runtime/core/utils/uuid.h"
#include <string>

namespace cakery {

class ReparentEntityCommand : public ICommand {
public:
    ReparentEntityCommand(dodoe::UUID entity, dodoe::UUID oldParent, dodoe::UUID newParent);

    bool execute(EditorContext& ctx) override;
    void undo(EditorContext& ctx) override;
    std::string label() const override;

private:
    dodoe::UUID m_entity;
    dodoe::UUID m_oldParent;
    dodoe::UUID m_newParent;
};

} // namespace cakery
