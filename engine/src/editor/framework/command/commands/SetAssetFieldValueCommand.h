#pragma once

#include "framework/command/ICommand.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/core/utils/json.h"
#include <string>

namespace cakery {

class SetAssetFieldValueCommand : public ICommand {
public:
    SetAssetFieldValueCommand(dodoe::UUID asset, std::string field,
                              dodoe::Json oldVal, dodoe::Json newVal);

    bool execute(EditorContext& ctx) override;
    void undo(EditorContext& ctx) override;
    std::string label() const override;
    bool mergeWith(const ICommand& next) override;

private:
    dodoe::UUID m_asset;
    std::string m_field;
    dodoe::Json m_old;
    dodoe::Json m_new;
};

} // namespace cakery
