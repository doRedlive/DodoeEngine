// do@Redlive

#pragma once

#include "core/commands/EditorCommand.h"

#include <cstdint>
#include <memory>
#include <string>

namespace cakery {

class ImportSpriteCommand final : public EditorCommand {
public:
    ImportSpriteCommand(std::string name, nlohmann::json spriteValue, nlohmann::json position);

    void execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;
    std::uint64_t createdUuid() const { return m_createdUuid; }
    bool mergeWith(const EditorCommand& next) override;

private:
    std::string m_name;
    nlohmann::json m_spriteValue;
    nlohmann::json m_position;
    std::uint64_t m_createdUuid = 0;
    bool m_built = false;
    std::unique_ptr<CreateEntityCommand> m_createEntity;
    std::unique_ptr<AddComponentCommand> m_addSprite;
    std::unique_ptr<UpdateComponentCommand> m_setTransform;
};

} // namespace cakery
