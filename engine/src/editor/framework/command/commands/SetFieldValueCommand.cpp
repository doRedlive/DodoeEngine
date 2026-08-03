// do@Redlive

#include "SetFieldValueCommand.h"
#include "framework/EditorContext.h"
#include "framework/core/UuidResolve.h"

#include "runtime/core/meta/component_db.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/meta/serializer/serializer.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/log/log_system.h"

#include <cstring>

namespace cakery {

namespace {

bool IsIntegerJson(const dodoe::Json& value)
{
    return value.is_number_integer() || value.is_number_unsigned();
}

dodoe::Json CaptureFieldValue(const char* typeName, void* value)
{
    if (!typeName || !typeName[0] || !value) return dodoe::Json();

    if (std::strcmp(typeName, "float") == 0) {
        return dodoe::Serializer::write(*static_cast<float*>(value));
    }
    if (std::strcmp(typeName, "double") == 0) {
        return dodoe::Serializer::write(*static_cast<double*>(value));
    }
    if (std::strcmp(typeName, "int") == 0 || std::strcmp(typeName, "int32_t") == 0) {
        return dodoe::Serializer::write(*static_cast<int*>(value));
    }
    if (std::strcmp(typeName, "unsigned int") == 0 || std::strcmp(typeName, "uint32_t") == 0) {
        return dodoe::Serializer::write(*static_cast<unsigned int*>(value));
    }
    if (std::strcmp(typeName, "bool") == 0) {
        return dodoe::Serializer::write(*static_cast<bool*>(value));
    }
    if (std::strcmp(typeName, "std::string") == 0) {
        return dodoe::Json(*static_cast<std::string*>(value));
    }
    if (std::strcmp(typeName, "String") == 0 || std::strcmp(typeName, "dodoe::String") == 0) {
        return dodoe::Serializer::write(*static_cast<dodoe::String*>(value));
    }
    if (std::strcmp(typeName, "UUID") == 0) {
        return dodoe::Serializer::write(*static_cast<dodoe::UUID*>(value));
    }
    if (std::strcmp(typeName, "Vector2f") == 0) return dodoe::Serializer::write(*static_cast<dodoe::Vector2f*>(value));
    if (std::strcmp(typeName, "Vector2i") == 0) return dodoe::Serializer::write(*static_cast<dodoe::Vector2i*>(value));
    if (std::strcmp(typeName, "Vector3f") == 0) return dodoe::Serializer::write(*static_cast<dodoe::Vector3f*>(value));
    if (std::strcmp(typeName, "Vector3i") == 0) return dodoe::Serializer::write(*static_cast<dodoe::Vector3i*>(value));
    if (std::strcmp(typeName, "Vector4f") == 0) return dodoe::Serializer::write(*static_cast<dodoe::Vector4f*>(value));
    if (std::strcmp(typeName, "Vector4i") == 0) return dodoe::Serializer::write(*static_cast<dodoe::Vector4i*>(value));
    if (std::strcmp(typeName, "Color") == 0) return dodoe::Serializer::write(*static_cast<dodoe::Color*>(value));

    return dodoe::TypeMeta::writeByName(typeName, value);
}

bool ApplyFieldValue(const char* typeName, dodoe::FieldAccessor& field, void* compPtr, const dodoe::Json& value)
{
    if (!typeName || !typeName[0] || value.is_null()) return false;

    if (std::strcmp(typeName, "float") == 0) {
        if (!value.is_number()) return false;
        float v = value.get<float>();
        field.set(compPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "double") == 0) {
        if (!value.is_number()) return false;
        double v = value.get<double>();
        field.set(compPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "int") == 0 || std::strcmp(typeName, "int32_t") == 0) {
        if (!IsIntegerJson(value)) return false;
        int v = value.get<int>();
        field.set(compPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "unsigned int") == 0 || std::strcmp(typeName, "uint32_t") == 0) {
        if (!IsIntegerJson(value)) return false;
        unsigned int v = value.get<unsigned int>();
        field.set(compPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "bool") == 0) {
        if (!value.is_boolean()) return false;
        bool v = value.get<bool>();
        field.set(compPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "std::string") == 0) {
        if (!value.is_string()) return false;
        std::string v = value.get<std::string>();
        field.set(compPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "String") == 0 || std::strcmp(typeName, "dodoe::String") == 0) {
        if (!value.is_string()) return false;
        std::string s = value.get<std::string>();
        dodoe::String v(s.data(), s.size());
        field.set(compPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "UUID") == 0) {
        if (!IsIntegerJson(value)) return false;
        dodoe::UUID v(static_cast<uint64_t>(value.get<uint64_t>()));
        field.set(compPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "Vector2f") == 0) {
        if (!value.is_array() || value.size() != 2) return false;
        dodoe::Vector2f v;
        dodoe::Serializer::read(value, v);
        field.set(compPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "Vector2i") == 0) {
        if (!value.is_array() || value.size() != 2) return false;
        dodoe::Vector2i v;
        dodoe::Serializer::read(value, v);
        field.set(compPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "Vector3f") == 0) {
        if (!value.is_array() || value.size() != 3) return false;
        dodoe::Vector3f v;
        dodoe::Serializer::read(value, v);
        field.set(compPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "Vector3i") == 0) {
        if (!value.is_array() || value.size() != 3) return false;
        dodoe::Vector3i v;
        dodoe::Serializer::read(value, v);
        field.set(compPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "Vector4f") == 0) {
        if (!value.is_array() || value.size() != 4) return false;
        dodoe::Vector4f v;
        dodoe::Serializer::read(value, v);
        field.set(compPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "Vector4i") == 0) {
        if (!value.is_array() || value.size() != 4) return false;
        dodoe::Vector4i v;
        dodoe::Serializer::read(value, v);
        field.set(compPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "Color") == 0) {
        if (!value.is_array() || value.size() != 4) return false;
        dodoe::Color v;
        dodoe::Serializer::read(value, v);
        field.set(compPtr, &v);
        return true;
    }

    dodoe::ReflectionInstance inst = dodoe::TypeMeta::newFromNameAndJson(typeName, value);
    if (!inst.instance) return false;
    field.set(compPtr, inst.instance);
    delete inst.instance;
    return true;
}

} // namespace

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
    void* compPtr = db.getComponentPtr(entity, m_component);
    if (!compPtr) return false;

    dodoe::TypeMeta meta = dodoe::TypeMeta::newMetaFromName(m_component);
    if (!meta.isValid()) return false;

    dodoe::FieldAccessor field = meta.get_field_by_name(m_field.c_str());
    const char* typeName = field.getFieldTypeName();
    if (!typeName || !typeName[0] || std::strcmp(typeName, "unknownType") == 0) return false;

    if (m_old.is_null()) {
        m_old = CaptureFieldValue(typeName, field.get(compPtr));
    }

    if (!ApplyFieldValue(typeName, field, compPtr, m_new)) return false;

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
    const char* typeName = field.getFieldTypeName();
    if (!typeName || !typeName[0] || std::strcmp(typeName, "unknownType") == 0) return;

    if (!ApplyFieldValue(typeName, field, compPtr, m_old)) return;

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
