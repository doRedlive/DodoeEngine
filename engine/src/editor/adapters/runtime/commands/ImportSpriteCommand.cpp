// do@Redlive

#include "ImportSpriteCommand.h"

#include "core/commands/EditorCommand.h"
#include "core/document/EditorDocumentModel.h"

#include <utility>

namespace cakery {

ImportSpriteCommand::ImportSpriteCommand(std::string name, nlohmann::json spriteValue,
                                         nlohmann::json position)
    : m_name(std::move(name))
    , m_spriteValue(std::move(spriteValue))
    , m_position(std::move(position))
{
}

void ImportSpriteCommand::execute(EditorDocumentModel& model)
{
    if (!m_built) {
        m_createEntity = std::make_unique<CreateEntityCommand>(m_name);
        m_createEntity->execute(model);
        m_createdUuid = m_createEntity->createdUuid();
        m_addSprite = std::make_unique<AddComponentCommand>(
            m_createdUuid, EditorComponent{"SpriteRendererComponent", m_spriteValue});
        m_addSprite->execute(model);
        nlohmann::json transform;
        transform["position"] = m_position;
        transform["rotation"] = nlohmann::json::array({0.0, 0.0, 0.0});
        transform["scale"] = nlohmann::json::array({1.0, 1.0, 1.0});
        m_setTransform = std::make_unique<UpdateComponentCommand>(m_createdUuid, 2, std::move(transform));
        m_setTransform->execute(model);
        m_built = true;
        return;
    }

    m_createEntity->execute(model);
    m_addSprite->execute(model);
    m_setTransform->execute(model);
}

void ImportSpriteCommand::revert(EditorDocumentModel& model)
{
    if (!m_built) {
        return;
    }
    m_setTransform->revert(model);
    m_addSprite->revert(model);
    m_createEntity->revert(model);
}

std::string ImportSpriteCommand::label() const
{
    return std::string("Import Sprite (") + m_name + ")";
}

bool ImportSpriteCommand::mergeWith(const EditorCommand& next)
{
    (void)next;
    return false;
}

} // namespace cakery
