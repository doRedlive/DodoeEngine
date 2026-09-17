// do@Redlive

#pragma once

#include "core/commands/EditorCommand.h"
#include "runtime/core/utils/uuid.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>

namespace cakery {

class InstantiatePrefabCommand final : public EditorCommand {
public:
    InstantiatePrefabCommand(std::string name, std::filesystem::path prefabPath,
                             nlohmann::json position);

    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

    dodoe::UUID created() const { return m_createdUuid; }
    void setCreatedUuid(dodoe::UUID uuid) { m_createdUuid = uuid; }

private:
    std::string m_name;
    std::filesystem::path m_prefabPath;
    nlohmann::json m_position;
    dodoe::UUID m_createdUuid;
    bool m_created = false;
};

} // namespace cakery
