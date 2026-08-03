// do@Redlive

#include "PropertyDrawer.h"
#include "drawers/ScalarDrawer.h"
#include "drawers/StringDrawer.h"
#include "drawers/VectorDrawer.h"
#include "drawers/ColorDrawer.h"
#include "drawers/EnumDrawer.h"
#include "drawers/PPtrDrawer.h"
#include "drawers/CompositeDrawer.h"

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
    const char* typeName = field.getFieldTypeName();
    if (!typeName) return nullptr;

    for (auto& [attr, factory] : m_byAttr) {
        if (field.hasAttribute(attr.c_str())) {
            return factory();
        }
    }

    auto it = m_byType.find(typeName);
    if (it != m_byType.end()) {
        return it->second();
    }

    dodoe::TypeMeta meta = dodoe::TypeMeta::newMetaFromName(typeName);
    if (meta.isValid()) {
        return std::make_unique<CompositeDrawer>();
    }

    return std::make_unique<ScalarDrawer>();
}

void PropertyDrawerRegistry::registerBuiltinDrawers()
{
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

    registerByType("CameraType", []() { return std::make_unique<EnumDrawer>(); });
    registerByType("dodoe::CameraType", []() { return std::make_unique<EnumDrawer>(); });

    auto pptrFactory = []() { return std::make_unique<PPtrDrawer>(); };
    registerByType("PPtr<Sprite>", pptrFactory);
}

} // namespace cakery
