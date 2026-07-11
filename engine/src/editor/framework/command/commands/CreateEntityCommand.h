// do@Redlive

#pragma once

#include "framework/command/ICommand.h"
#include "runtime/core/utils/uuid.h"
#include <string>
#include <optional>

namespace cakery {

class CreateEntityCommand : public ICommand {
public:
    CreateEntityCommand(dodoe::Uuid uuid, std::string name,
                        std::optional<dodoe::Uuid> parent = std::nullopt);

    bool execute(EditorContext& ctx) override;
    void undo(EditorContext& ctx) override;
    void redo(EditorContext& ctx) override;
    std::string label() const override;

private:
    dodoe::Uuid m_uuid;
    std::string m_name;
    std::optional<dodoe::Uuid> m_parent;
};

} // namespace cakery
