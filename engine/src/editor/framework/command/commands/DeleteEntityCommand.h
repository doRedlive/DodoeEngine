// do@Redlive

#pragma once

#include "framework/command/ICommand.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/core/utils/json.h"
#include <string>
#include <memory>

namespace dodoe { struct SceneRes; }

namespace cakery {

class DeleteEntityCommand : public ICommand {
public:
    explicit DeleteEntityCommand(dodoe::UUID uuid);

    bool execute(EditorContext& ctx) override;
    void undo(EditorContext& ctx) override;
    std::string label() const override;

private:
    dodoe::UUID m_uuid;
    std::unique_ptr<dodoe::SceneRes> m_serializedSubtree;
};

} // namespace cakery
