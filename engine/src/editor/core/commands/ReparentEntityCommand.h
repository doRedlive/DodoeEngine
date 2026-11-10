// do@Redlive
#pragma once
#include "dopch.h"

#include "core/commands/EditorCommand.h"
#include "runtime/core/utils/uuid.h"

#include <string>

namespace cakery {

class ReparentEntityCommand final : public EditorCommand {
public:
    ReparentEntityCommand(dodoe::UUID entity, dodoe::UUID oldParent, dodoe::UUID newParent);

    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;

private:
    bool doReparent(EditorDocumentModel& model, dodoe::UUID newParent);

    dodoe::UUID m_entity;
    dodoe::UUID m_oldParent;
    dodoe::UUID m_newParent;
};

class CommandRegistry;

void RegisterReparentCommand(CommandRegistry& registry);

} // namespace cakery
