#pragma once

#include <string>
#include <vector>

namespace dodoe {
    class TypeMeta;
}

namespace cakery {

class FieldAttributeRegistry {
public:
    static FieldAttributeRegistry& self();

    void registerDefaults();
    void applyTo(dodoe::TypeMeta& meta, const std::string& typeName);

private:
    struct DefaultAttribute {
        const char* typeName;
        const char* fieldName;
        const char* key;
        const char* value;
    };

    FieldAttributeRegistry() = default;
    bool m_defaultsRegistered = false;
    std::vector<DefaultAttribute> m_defaults;
};

} // namespace cakery
