// do@Redlive

#include "PropertyDrawer.h"
#include "drawers/ScalarDrawer.h"
#include "drawers/StringDrawer.h"
#include "drawers/VectorDrawer.h"
#include "drawers/ColorDrawer.h"
#include "drawers/EnumDrawer.h"
#include "drawers/PPtrDrawer.h"
#include "drawers/AssetHandleDrawer.h"
#include "drawers/CompositeDrawer.h"
#include "drawers/ArrayDrawer.h"
#include "drawers/ReadOnlyDrawer.h"

#include "runtime/core/meta/reflection/reflection.h"

namespace cakery {

PropertyDrawerRegistry& PropertyDrawerRegistry::self()
{
    static PropertyDrawerRegistry instance;
    return instance;
}

void PropertyDrawerRegistry::registerByType(const std::string& typeName, Factory f)
{
    m_byType[typeName] = std::move(f);
}

void PropertyDrawerRegistry::registerByAttribute(const std::string& attrKey, Factory f)
{
    m_byAttr[attrKey] = std::move(f);
}

std::unique_ptr<PropertyDrawer> PropertyDrawerRegistry::create(dodoe::FieldAccessor& field)
{
    for (auto& [attr, factory] : m_byAttr) {
        if (field.hasAttribute(attr.c_str())) {
            return factory();
        }
    }

    switch (field.getFieldType()) {
    case dodoe::FieldType::Bool:
    case dodoe::FieldType::I32:
    case dodoe::FieldType::U32:
    case dodoe::FieldType::F32:
    case dodoe::FieldType::F64:
        return std::make_unique<ScalarDrawer>();
    case dodoe::FieldType::String:
        return std::make_unique<StringDrawer>();
    case dodoe::FieldType::Enum:
        return std::make_unique<EnumDrawer>();
    case dodoe::FieldType::Vec2:
    case dodoe::FieldType::Vec2i:
        return std::make_unique<VectorDrawer<2>>();
    case dodoe::FieldType::Vec3:
    case dodoe::FieldType::Vec3i:
        return std::make_unique<VectorDrawer<3>>();
    case dodoe::FieldType::Vec4:
    case dodoe::FieldType::Vec4i:
        return std::make_unique<VectorDrawer<4>>();
    case dodoe::FieldType::Color:
        return std::make_unique<ColorDrawer>();
    case dodoe::FieldType::Ptr:
        return std::make_unique<PPtrDrawer>();
    case dodoe::FieldType::Array:
        return std::make_unique<ArrayDrawer>();
    case dodoe::FieldType::Struct: {
        const char* typeName = field.getFieldTypeName();
        if (typeName && dodoe::TypeMeta::newMetaFromName(typeName).isValid()) {
            return std::make_unique<CompositeDrawer>();
        }
        return std::make_unique<ReadOnlyDrawer>();
    }
    case dodoe::FieldType::Unknown:
    default:
        break;
    }

    const char* typeName = field.getFieldTypeName();
    if (typeName) {
        auto it = m_byType.find(typeName);
        if (it != m_byType.end()) {
            return it->second();
        }
    }

    return std::make_unique<ReadOnlyDrawer>();
}

void PropertyDrawerRegistry::registerBuiltinDrawers()
{
    registerByAttribute("AssetHandle", []() { return std::make_unique<AssetHandleDrawer>(); });
    registerByType("float",     []() { return std::make_unique<ScalarDrawer>(); });
    registerByType("double",    []() { return std::make_unique<ScalarDrawer>(); });
    registerByType("int",       []() { return std::make_unique<ScalarDrawer>(); });
    registerByType("int32_t",   []() { return std::make_unique<ScalarDrawer>(); });
    registerByType("uint32_t",  []() { return std::make_unique<ScalarDrawer>(); });
    registerByType("bool",      []() { return std::make_unique<ScalarDrawer>(); });
    registerByType("std::string", []() { return std::make_unique<StringDrawer>(); });
    registerByType("Vector2f",  []() { return std::make_unique<VectorDrawer<2>>(); });
    registerByType("Vector2i",  []() { return std::make_unique<VectorDrawer<2>>(); });
    registerByType("Vector3f",  []() { return std::make_unique<VectorDrawer<3>>(); });
    registerByType("Vector3i",  []() { return std::make_unique<VectorDrawer<3>>(); });
    registerByType("Vector4f",  []() { return std::make_unique<VectorDrawer<4>>(); });
    registerByType("Vector4i",  []() { return std::make_unique<VectorDrawer<4>>(); });
    registerByType("Color",     []() { return std::make_unique<ColorDrawer>(); });
}

} // namespace cakery
