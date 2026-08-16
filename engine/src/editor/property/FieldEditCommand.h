// do@Redlive

#pragma once

#include "property/PropertyDrawer.h"
#include "framework/command/ICommand.h"
#include "framework/command/commands/SetFieldValueCommand.h"
#include "framework/command/commands/SetAssetFieldValueCommand.h"
#include "runtime/core/utils/json.h"

#include <memory>
#include <string>

namespace cakery {

inline std::unique_ptr<ICommand> MakeFieldEditCommand(
    const PropertyContext& pc, const std::string& fieldName, dodoe::Json oldVal, dodoe::Json newVal)
{
    if (pc.isAsset) {
        return std::make_unique<SetAssetFieldValueCommand>(pc.entity, fieldName, std::move(oldVal), std::move(newVal));
    }
    return std::make_unique<SetFieldValueCommand>(pc.entity, pc.componentName, fieldName, std::move(oldVal), std::move(newVal));
}

} // namespace cakery
