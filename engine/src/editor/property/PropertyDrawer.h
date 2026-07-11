// do@Redlive

#pragma once

#include <QWidget>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "runtime/core/utils/uuid.h"

namespace dodoe {
    class FieldAccessor;
}

namespace cakery {

class EditorContext;

struct PropertyContext {
    EditorContext*           ctx         = nullptr;
    dodoe::Uuid              entity;
    std::string              componentName;
    void*                    componentPtr = nullptr;
    dodoe::FieldAccessor*    field        = nullptr;
};

class PropertyDrawer {
public:
    virtual ~PropertyDrawer() = default;
    virtual QWidget* build(const PropertyContext& pc) = 0;
    virtual void updateValue(const PropertyContext& pc) = 0;
};

class PropertyDrawerRegistry {
public:
    static PropertyDrawerRegistry& self();

    using Factory = std::function<std::unique_ptr<PropertyDrawer>()>;

    void registerByType(const std::string& typeName, Factory f);
    void registerByAttribute(const std::string& attrKey, Factory f);

    std::unique_ptr<PropertyDrawer> create(dodoe::FieldAccessor& field);

    void registerBuiltinDrawers();

private:
    PropertyDrawerRegistry() = default;

    std::unordered_map<std::string, Factory> m_byType;
    std::unordered_map<std::string, Factory> m_byAttr;
};

} // namespace cakery
