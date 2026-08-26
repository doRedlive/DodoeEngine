// do@Redlive

#pragma once

#include "core/commands/EditorCommand.h"

#include <cstdint>
#include <memory>
#include <string>

namespace cakery {

class ImportMeshCommand final : public EditorCommand {
public:
    ImportMeshCommand(std::string name, nlohmann::json meshValue, nlohmann::json position);

    void execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;
    std::uint64_t createdUuid() const { return m_createdUuid; }
    bool mergeWith(const EditorCommand& next) override;

private:
    std::string m_name;
    nlohmann::json m_meshValue;
    nlohmann::json m_position;
    std::uint64_t m_createdUuid = 0;
    bool m_built = false;
    std::unique_ptr<CreateEntityCommand> m_createEntity;
    std::unique_ptr<AddComponentCommand> m_addMesh;
    std::unique_ptr<UpdateComponentCommand> m_setTransform;
};

} // namespace cakery
