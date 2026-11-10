#include "dopch.h"

#pragma once

#include <string>
#include <vector>

namespace dodoe {
    class TypeMeta;
}

namespace cakery {

class FieldAttributeRegistry {
public:
    FieldAttributeRegistry();

    void registerDefaults();
    void applyTo(dodoe::TypeMeta& meta, const String& typeName, const dodoe::Json& inspectors);

private:
    struct DefaultAttribute {
        const char* typeName;
        const char* fieldName;
        const char* key;
        const char* value;
    };

    bool m_defaultsRegistered = false;
    DynamicArray<DefaultAttribute> m_defaults;
};

} // namespace cakery
