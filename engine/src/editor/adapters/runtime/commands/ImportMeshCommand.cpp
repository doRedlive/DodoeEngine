// do@Redlive

#include "ImportMeshCommand.h"

#include "core/commands/EditorCommand.h"
#include "core/document/EditorDocumentModel.h"

#include <utility>

namespace cakery {

ImportMeshCommand::ImportMeshCommand(std::string name, nlohmann::json meshValue,
                                     nlohmann::json position)
    : m_name(std::move(name))
    , m_meshValue(std::move(meshValue))
    , m_position(std::move(position))
{
}

void ImportMeshCommand::execute(EditorDocumentModel& model)
{
    if (!m_built) {
        m_createEntity = std::make_unique<CreateEntityCommand>(m_name);
        m_createEntity->execute(model);
        m_createdUuid = m_createEntity->createdUuid();
        m_addMesh = std::make_unique<AddComponentCommand>(
            m_createdUuid, EditorComponent{"MeshRendererComponent", m_meshValue});
        m_addMesh->execute(model);
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
    m_addMesh->execute(model);
    m_setTransform->execute(model);
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

std::string ImportMeshCommand::label() const
{
    return std::string("Import Mesh (") + m_name + ")";
}

bool ImportMeshCommand::mergeWith(const EditorCommand& next)
{
    (void)next;
    return false;
}

} // namespace cakery
