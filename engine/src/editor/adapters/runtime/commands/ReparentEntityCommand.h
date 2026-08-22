// do@Redlive

#pragma once

#include "core/commands/EditorCommand.h"
#include "runtime/core/utils/uuid.h"

#include <string>

namespace cakery {

class ReparentEntityCommand final : public EditorCommand {
public:
    ReparentEntityCommand(dodoe::UUID entity, dodoe::UUID oldParent, dodoe::UUID newParent);

    void execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

private:
    void doReparent(EditorDocumentModel& model, dodoe::UUID newParent);

    dodoe::UUID m_entity;
    dodoe::UUID m_oldParent;
    dodoe::UUID m_newParent;
};

void RegisterReparentCommand();

} // namespace cakery
