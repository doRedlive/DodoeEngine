// do@Redlive

#pragma once

#include "framework/command/ICommand.h"
#include "runtime/core/utils/uuid.h"
#include <string>

namespace cakery {

class ReparentEntityCommand : public ICommand {
public:
    ReparentEntityCommand(dodoe::Uuid entity, dodoe::Uuid oldParent, dodoe::Uuid newParent);

    bool execute(EditorContext& ctx) override;
    void undo(EditorContext& ctx) override;
    std::string label() const override;

private:
    dodoe::Uuid m_entity;
    dodoe::Uuid m_oldParent;
    dodoe::Uuid m_newParent;
};

} // namespace cakery
