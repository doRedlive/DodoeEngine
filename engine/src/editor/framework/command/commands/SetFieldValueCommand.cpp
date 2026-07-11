// do@Redlive

#include "SetFieldValueCommand.h"
#include "framework/EditorContext.h"
#include "framework/core/UuidResolve.h"

#include "runtime/core/meta/component_db.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/log/log_system.h"

namespace cakery {

SetFieldValueCommand::SetFieldValueCommand(dodoe::Uuid entity, std::string component,
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
    void* compPtr = db.getComponentPtr(entity, m_component);
    if (!compPtr) return false;

    dodoe::TypeMeta meta = dodoe::TypeMeta::newMetaFromName(m_component);
    if (!meta.isValid()) return false;

    dodoe::FieldAccessor field = meta.get_field_by_name(m_field.c_str());
    if (field.getFieldName() == nullptr) return false;

    dodoe::Json temp = m_new;
    dodoe::ReflectionInstance inst = dodoe::TypeMeta::newFromNameAndJson(m_field, temp);
    if (inst.instance) {
        field.set(compPtr, inst.instance);
    }

    db.markComponentDirty(entity, m_component);
    return true;
}

void SetFieldValueCommand::undo(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return;

    auto entity = ResolveEntity(scene, m_entity);
    if (!entity.valid()) return;

    auto& db = dodoe::ComponentDB::self();
    void* compPtr = db.getComponentPtr(entity, m_component);
    if (!compPtr) return;

    dodoe::TypeMeta meta = dodoe::TypeMeta::newMetaFromName(m_component);
    if (!meta.isValid()) return;

    dodoe::FieldAccessor field = meta.get_field_by_name(m_field.c_str());
    if (field.getFieldName() == nullptr) return;

    dodoe::Json temp = m_old;
    dodoe::ReflectionInstance inst = dodoe::TypeMeta::newFromNameAndJson(m_field, temp);
    if (inst.instance) {
        field.set(compPtr, inst.instance);
    }

    db.markComponentDirty(entity, m_component);
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
