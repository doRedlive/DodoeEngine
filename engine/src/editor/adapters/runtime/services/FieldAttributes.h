#pragma once

#include <string>

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
    FieldAttributeRegistry() = default;
};

} // namespace cakery
