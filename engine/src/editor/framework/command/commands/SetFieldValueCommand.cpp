// do@Redlive

#include "SetFieldValueCommand.h"
#include "framework/EditorContext.h"
#include "framework/command/FieldValueUtils.h"
#include "framework/core/UuidResolve.h"

#include "runtime/core/meta/component_db.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/log/log_system.h"

#include <cstring>

namespace cakery {

SetFieldValueCommand::SetFieldValueCommand(dodoe::UUID entity, std::string component,
                                           std::string field, dodoe::Json oldVal, dodoe::Json newVal)
    : m_entity(entity)
    , m_component(std::move(component))
    , m_field(std::move(field))
    , m_old(std::move(oldVal))
    , m_new(std::move(newVal))
{}

bool SetFieldValueCommand::execute(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return false;

    auto entity = ResolveEntity(scene, m_entity);
    if (!entity.valid()) return false;

    auto& db = dodoe::ComponentDB::self();
    void* compPtr = db.getComponentPtr(entity, dodoe::String(m_component.c_str()));
    if (!compPtr) return false;

    dodoe::TypeMeta meta = dodoe::TypeMeta::newMetaFromName(dodoe::String(m_component.c_str()));
    if (!meta.isValid()) return false;

    dodoe::FieldAccessor field = meta.get_field_by_name(m_field.c_str());
    const char* typeName = field.getFieldTypeName();
    if (!typeName || !typeName[0] || std::strcmp(typeName, "unknownType") == 0) return false;

    if (m_old.is_null()) {
        m_old = CaptureFieldValue(typeName, field.get(compPtr));
    }

    if (!ApplyFieldValue(typeName, field, compPtr, m_new)) return false;

    db.markComponentDirty(entity, dodoe::String(m_component.c_str()));
    return true;
}

void SetFieldValueCommand::undo(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return;

    auto entity = ResolveEntity(scene, m_entity);
    if (!entity.valid()) return;

    auto& db = dodoe::ComponentDB::self();
    void* compPtr = db.getComponentPtr(entity, dodoe::String(m_component.c_str()));
    if (!compPtr) return;

    dodoe::TypeMeta meta = dodoe::TypeMeta::newMetaFromName(dodoe::String(m_component.c_str()));
    if (!meta.isValid()) return;

    dodoe::FieldAccessor field = meta.get_field_by_name(m_field.c_str());
    const char* typeName = field.getFieldTypeName();
    if (!typeName || !typeName[0] || std::strcmp(typeName, "unknownType") == 0) return;

    if (!ApplyFieldValue(typeName, field, compPtr, m_old)) return;

    db.markComponentDirty(entity, dodoe::String(m_component.c_str()));
}

std::string SetFieldValueCommand::label() const
{
    return "Modify " + m_component + "." + m_field;
}

bool SetFieldValueCommand::mergeWith(const ICommand& next)
{
    auto* n = dynamic_cast<const SetFieldValueCommand*>(&next);
    if (!n || n->m_entity != m_entity || n->m_component != m_component
        || n->m_field != m_field) {
        return false;
    }
    m_new = n->m_new;
    return true;
}

} // namespace cakery
