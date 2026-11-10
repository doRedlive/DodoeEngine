// do@Redlive

#include "dopch.h"

#include "ImportSpriteCommand.h"

#include "core/commands/EditorCommand.h"
#include "core/document/EditorDocumentModel.h"

#include <utility>

namespace cakery {

ImportSpriteCommand::ImportSpriteCommand(String name, Json spriteValue,
                                         Json position)
    : m_name(std::move(name))
    , m_spriteValue(std::move(spriteValue))
    , m_position(std::move(position))
{
}

bool ImportSpriteCommand::execute(EditorDocumentModel& model)
{
    if (!m_built) {
        m_createEntity = create_scope<CreateEntityCommand>(m_name);
        if (!m_createEntity->execute(model)) {
            return false;
        }
        m_createdUuid = m_createEntity->createdUuid();
        m_addSprite = create_scope<AddComponentCommand>(
            m_createdUuid, EditorComponent{"SpriteRendererComponent", m_spriteValue});
        if (!m_addSprite->execute(model)) {
            m_createEntity->revert(model);
            return false;
        }
        Json transform;
        transform["position"] = m_position;
        transform["rotation"] = Json::array({0.0, 0.0, 0.0});
        transform["scale"] = Json::array({1.0, 1.0, 1.0});
        m_setTransform = create_scope<UpdateComponentCommand>(m_createdUuid, 2, std::move(transform));
        if (!m_setTransform->execute(model)) {
            m_addSprite->revert(model);
            m_createEntity->revert(model);
            return false;
        }
        m_built = true;
        return true;
    }

    return m_createEntity->execute(model) && m_addSprite->execute(model) && m_setTransform->execute(model);
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

String ImportSpriteCommand::label() const
{
    return String("Import Sprite (") + m_name + ")";
}

bool ImportSpriteCommand::mergeWith(const EditorCommand& next)
{
    (void)next;
    return false;
}

} // namespace cakery
