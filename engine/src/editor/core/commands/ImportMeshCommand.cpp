// do@Redlive

#include "dopch.h"

#include "ImportMeshCommand.h"

#include "core/commands/EditorCommand.h"
#include "core/document/EditorDocumentModel.h"

#include <utility>

namespace cakery {

ImportMeshCommand::ImportMeshCommand(String name, Json meshValue,
                                     Json position)
    : m_name(std::move(name))
    , m_meshValue(std::move(meshValue))
    , m_position(std::move(position))
{
}

bool ImportMeshCommand::execute(EditorDocumentModel& model)
{
    if (!m_built) {
        m_createEntity = create_scope<CreateEntityCommand>(m_name);
        if (!m_createEntity->execute(model)) {
            return false;
        }
        m_createdUuid = m_createEntity->createdUuid();
        m_addMesh = create_scope<AddComponentCommand>(
            m_createdUuid, EditorComponent{"MeshRendererComponent", m_meshValue});
        if (!m_addMesh->execute(model)) {
            m_createEntity->revert(model);
            return false;
        }
        Json transform;
        transform["position"] = m_position;
        transform["rotation"] = Json::array({0.0, 0.0, 0.0});
        transform["scale"] = Json::array({1.0, 1.0, 1.0});
        m_setTransform = create_scope<UpdateComponentCommand>(m_createdUuid, 2, std::move(transform));
        if (!m_setTransform->execute(model)) {
            m_addMesh->revert(model);
            m_createEntity->revert(model);
            return false;
        }
        m_built = true;
        return true;
    }

    return m_createEntity->execute(model) && m_addMesh->execute(model) && m_setTransform->execute(model);
}

void ImportMeshCommand::revert(EditorDocumentModel& model)
{
    if (!m_built) {
        return;
    }
    m_setTransform->revert(model);
    m_addMesh->revert(model);
    m_createEntity->revert(model);
}

String ImportMeshCommand::label() const
{
    return String("Import Mesh (") + m_name + ")";
}

bool ImportMeshCommand::mergeWith(const EditorCommand& next)
{
    (void)next;
    return false;
}

} // namespace cakery
