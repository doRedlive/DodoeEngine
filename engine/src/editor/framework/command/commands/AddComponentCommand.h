// do@Redlive

#pragma once

#include "framework/command/ICommand.h"
#include "runtime/core/utils/uuid.h"
#include <string>

namespace cakery {

class AddComponentCommand : public ICommand {
public:
    AddComponentCommand(dodoe::Uuid entity, std::string componentName);

    bool execute(EditorContext& ctx) override;
    void undo(EditorContext& ctx) override;
    std::string label() const override;

private:
    dodoe::Uuid m_entity;
    std::string m_componentName;
};

} // namespace cakery
