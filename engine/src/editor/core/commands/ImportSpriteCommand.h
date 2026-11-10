// do@Redlive
#pragma once
#include "dopch.h"

#include "core/commands/EditorCommand.h"

#include <cstdint>
#include <memory>
#include <string>

namespace cakery {

class ImportSpriteCommand final : public EditorCommand {
public:
    ImportSpriteCommand(String name, Json spriteValue, Json position);

    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;
    UInt64 createdUuid() const { return m_createdUuid; }
    bool mergeWith(const EditorCommand& next) override;

private:
    String m_name;
    Json m_spriteValue;
    Json m_position;
    UInt64 m_createdUuid = 0;
    bool m_built = false;
    Scope<CreateEntityCommand> m_createEntity;
    Scope<AddComponentCommand> m_addSprite;
    Scope<UpdateComponentCommand> m_setTransform;
};

} // namespace cakery
