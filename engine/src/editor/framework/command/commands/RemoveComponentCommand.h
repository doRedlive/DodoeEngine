// do@Redlive

#pragma once

#include "framework/command/ICommand.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/core/utils/json.h"
#include <string>

namespace cakery {

class RemoveComponentCommand : public ICommand {
public:
    RemoveComponentCommand(dodoe::UUID entity, std::string componentName);

    bool execute(EditorContext& ctx) override;
    void undo(EditorContext& ctx) override;
    std::string label() const override;

private:
    dodoe::UUID m_entity;
    std::string m_componentName;
    dodoe::Json m_serializedComponent;
};

} // namespace cakery
