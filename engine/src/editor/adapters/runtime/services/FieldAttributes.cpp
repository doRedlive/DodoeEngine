// do@Redlive

#include "FieldAttributes.h"
#include "services/EditorConfig.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/json.h"

#include <cstring>

namespace cakery {

FieldAttributeRegistry& FieldAttributeRegistry::self() {
    static FieldAttributeRegistry instance;
    return instance;
}

void FieldAttributeRegistry::registerDefaults() {
    if (m_defaultsRegistered) {
        return;
    }
    m_defaultsRegistered = true;

    // These are editor hints only; the runtime serializer remains the source
    // of truth for the component JSON representation.
    m_defaults = {
        {"CameraComponent", "fov", "Range", "1,179"},
        {"CameraComponent", "near_plane", "Range", "0.001,1000"},
        {"CameraComponent", "far_plane", "Range", "1,100000"},
        {"Rigidbody2dComponent", "gravity_scale", "Range", "0,10"},
        {"RigidbodyComponent", "gravity_scale", "Range", "0,10"},
    };
}

void FieldAttributeRegistry::applyTo(dodoe::TypeMeta& meta, const std::string& typeName) {
    registerDefaults();

    for (const auto& attr : m_defaults) {
        if (typeName == attr.typeName) {
            meta.set_field_attribute(attr.fieldName, attr.key, attr.value);
        }
    }

    auto& inspectors = EditorConfig::self().inspectorsJson();
    if (inspectors.contains("fieldAttributes")) {
        auto& attrs = inspectors["fieldAttributes"];
        for (auto it = attrs.begin(); it != attrs.end(); ++it) {
            const std::string prefix = typeName + ".";
            if (it.key().rfind(prefix, 0) != 0 || !it.value().is_object()) {
                continue;
            }
            const std::string fieldName = it.key().substr(prefix.size());
            const auto& fa = it.value();
            if (fa.contains("Hidden") && fa["Hidden"].is_boolean() && fa["Hidden"].get<bool>()) {
                meta.set_field_attribute(fieldName.c_str(), "Hidden", "true");
            }
            if (fa.contains("Tooltip") && fa["Tooltip"].is_string()) {
                const std::string tooltip = fa["Tooltip"].get<std::string>();
                meta.set_field_attribute(fieldName.c_str(), "Tooltip", tooltip.c_str());
            }
            if (fa.contains("Range") && fa["Range"].is_array() && fa["Range"].size() == 2 &&
                fa["Range"][0].is_number() && fa["Range"][1].is_number()) {
                const std::string rangeStr = std::to_string(fa["Range"][0].get<float>()) + "," +
                                              std::to_string(fa["Range"][1].get<float>());
                meta.set_field_attribute(fieldName.c_str(), "Range", rangeStr.c_str());
            }
            if (fa.contains("ReadOnly") && fa["ReadOnly"].is_boolean() && fa["ReadOnly"].get<bool>()) {
                meta.set_field_attribute(fieldName.c_str(), "ReadOnly", "true");
            }
        }
    }
}

} // namespace cakery
