// do@Redlive

#include "FieldAttributes.h"
#include "framework/config/EditorConfig.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/json.h"

#include <cstring>

namespace cakery {

FieldAttributeRegistry& FieldAttributeRegistry::self() {
    static FieldAttributeRegistry instance;
    return instance;
}

void FieldAttributeRegistry::registerDefaults() {
}

void FieldAttributeRegistry::applyTo(dodoe::TypeMeta& meta, const std::string& typeName) {
    dodoe::FieldAccessor* fields = nullptr;
    int count = meta.get_field_list(fields);

    auto& inspectors = EditorConfig::self().inspectorsJson();
    if (inspectors.contains("fieldAttributes")) {
        auto& attrs = inspectors["fieldAttributes"];
        for (int i = 0; i < count; ++i) {
            std::string key = typeName + "." + std::string(fields[i].getFieldName());
            if (attrs.contains(key)) {
                auto& fa = attrs[key];
                if (fa.contains("Hidden") && fa["Hidden"].get<bool>()) {
                    fields[i].setAttribute("Hidden", "true");
                }
                if (fa.contains("Tooltip") && fa["Tooltip"].is_string()) {
                    fields[i].setAttribute("Tooltip", fa["Tooltip"].get<std::string>().c_str());
                }
                if (fa.contains("Range") && fa["Range"].is_array() && fa["Range"].size() == 2) {
                    std::string rangeStr = std::to_string(fa["Range"][0].get<float>()) + "," +
                                           std::to_string(fa["Range"][1].get<float>());
                    fields[i].setAttribute("Range", rangeStr.c_str());
                }
                if (fa.contains("ReadOnly") && fa["ReadOnly"].get<bool>()) {
                    fields[i].setAttribute("ReadOnly", "true");
                }
            }
        }
    }

    delete[] fields;
}

} // namespace cakery
