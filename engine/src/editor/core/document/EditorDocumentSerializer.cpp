// do@Redlive

#include "dopch.h"

#include "EditorDocumentSerializer.h"

#include <algorithm>
#include <fstream>
#include <unordered_set>
#include <utility>

namespace cakery {

namespace {

DynamicArray<EditorComponent> ReadComponents(const Json& entityJson, const char* key) {
    DynamicArray<EditorComponent> components;
    if (!entityJson.contains(key) || !entityJson[key].is_array()) {
        return components;
    }
    for (const auto& item : entityJson[key]) {
        EditorComponent component;
        if (item.contains("m_type_name") && item["m_type_name"].is_string()) {
            component.typeName = item["m_type_name"].get<String>();
        }
        if (item.contains("m_component") && item["m_component"].is_string()) {
            try {
                component.value = Json::parse(item["m_component"].get<String>());
            } catch (const Json::exception&) {
                component.value = Json::object();
            }
        }
        components.push_back(std::move(component));
    }
    return components;
}

UInt64 ParentFromHierarchyComponent(const DynamicArray<EditorComponent>& components)
{
    for (const auto& component : components) {
        if (component.typeName != "HierarchyComponent" || !component.value.is_object()) {
            continue;
        }
        const auto it = component.value.find("parent_uuid");
        if (it != component.value.end() &&
            (it->is_number_unsigned() ||
             (it->is_number_integer() && it->get<Int64>() >= 0))) {
            return it->get<UInt64>();
        }
    }
    return 0;
}

Json WriteComponents(const DynamicArray<EditorComponent>& components) {
    Json array = Json::array();
    for (const auto& component : components) {
        Json item;
        item["m_type_name"] = component.typeName;
        item["m_component"] = component.value.dump();
        array.push_back(std::move(item));
    }
    return array;
}

bool ValidComponents(const Json& entityJson, const char* key)
{
    if (!entityJson.contains(key)) {
        return true;
    }
    if (!entityJson[key].is_array()) {
        return false;
    }
    for (const auto& item : entityJson[key]) {
        if (!item.is_object() || !item.contains("m_type_name") ||
            !item["m_type_name"].is_string() || item["m_type_name"].get<String>().empty() ||
            !item.contains("m_component") || !item["m_component"].is_string()) {
            return false;
        }
        const auto parsed = Json::parse(item["m_component"].get<String>(), nullptr, false);
        if (parsed.is_discarded()) {
            return false;
        }
    }
    return true;
}

} // namespace

Json EditorDocumentSerializer::toJson(const EditorDocument& document)
{
    Json root;
    root["m_name"] = document.name;

    Json entities = Json::array();
    for (const auto& entity : document.entities) {
        Json entityJson;
        entityJson["m_uuid"] = entity.uuid;
        entityJson["m_parent"] = entity.parent;
        entityJson["m_name"] = entity.name;
        entityJson["m_native_components"] = WriteComponents(entity.nativeComponents);
        entityJson["m_managed_components"] = WriteComponents(entity.managedComponents);
        entities.push_back(std::move(entityJson));
    }
    root["m_entities"] = std::move(entities);
    return root;
}

bool EditorDocumentSerializer::fromJson(const Json& root, EditorDocument& outDocument)
{
    if (!root.is_object() || (root.contains("m_entities") && !root["m_entities"].is_array())) {
        return false;
    }
    EditorDocument document;
    if (root.contains("m_name") && root["m_name"].is_string()) {
        document.name = root["m_name"].get<String>();
    }
    if (root.contains("m_entities") && root["m_entities"].is_array()) {
        for (const auto& entityJson : root["m_entities"]) {
            if (!entityJson.is_object() || !entityJson.contains("m_uuid") ||
                !(entityJson["m_uuid"].is_number_unsigned() || entityJson["m_uuid"].is_number_integer())) {
                return false;
            }
            if ((entityJson.contains("m_parent") &&
                 !(entityJson["m_parent"].is_number_unsigned() || entityJson["m_parent"].is_number_integer())) ||
                (entityJson.contains("m_parent") && entityJson["m_parent"].is_number_integer() &&
                 entityJson["m_parent"].get<Int64>() < 0) ||
                !ValidComponents(entityJson, "m_native_components") ||
                !ValidComponents(entityJson, "m_managed_components")) {
                return false;
            }
            EditorEntity entity;
            if (entityJson["m_uuid"].is_number_integer() && entityJson["m_uuid"].get<Int64>() <= 0) {
                return false;
            }
            entity.uuid = entityJson["m_uuid"].get<UInt64>();
            if (entity.uuid == 0) {
                return false;
            }
            const bool hasSerializedParent = entityJson.contains("m_parent") &&
                (entityJson["m_parent"].is_number_unsigned() || entityJson["m_parent"].is_number_integer());
            if (hasSerializedParent) {
                entity.parent = entityJson["m_parent"].get<UInt64>();
            }
            if (entityJson.contains("m_name") && entityJson["m_name"].is_string()) {
                entity.name = entityJson["m_name"].get<String>();
            }
            entity.nativeComponents = ReadComponents(entityJson, "m_native_components");
            entity.managedComponents = ReadComponents(entityJson, "m_managed_components");
            if (!hasSerializedParent || entity.parent == 0) {
                entity.parent = ParentFromHierarchyComponent(entity.nativeComponents);
            }
            document.entities.push_back(std::move(entity));
        }
    }

    UnorderedSet<UInt64> uuids;
    for (const auto& entity : document.entities) {
        if (!uuids.insert(entity.uuid).second) {
            return false;
        }
    }
    for (const auto& entity : document.entities) {
        if (entity.parent != 0 && !uuids.contains(entity.parent)) {
            return false;
        }
        UnorderedSet<UInt64> visited;
        for (UInt64 ancestor = entity.uuid; ancestor != 0;) {
            if (!visited.insert(ancestor).second) {
                return false;
            }
            const auto it = std::find_if(document.entities.begin(), document.entities.end(),
                                         [ancestor](const EditorEntity& candidate) {
                                             return candidate.uuid == ancestor;
                                         });
            if (it == document.entities.end()) {
                break;
            }
            ancestor = it->parent;
        }
    }

    outDocument = std::move(document);
    return true;
}

bool EditorDocumentSerializer::load(const FsPath& path, EditorDocument& outDocument) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    Json root;
    try {
        file >> root;
    } catch (const Json::exception&) {
        return false;
    }

    try {
        return fromJson(root, outDocument);
    } catch (const Json::exception&) {
        return false;
    }
}

bool EditorDocumentSerializer::save(const EditorDocument& document, const FsPath& path) {
    if (path.empty()) {
        return false;
    }
    Json root = toJson(document);

    const FsPath temporary = path.string() + ".tmp";
    const FsPath backup = path.string() + ".bak";
    std::error_code ec;
    std::filesystem::remove(temporary, ec);
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    file << root.dump(4) << std::endl;
    file.flush();
    if (!file.good()) {
        file.close();
        std::filesystem::remove(temporary, ec);
        return false;
    }
    file.close();

    const bool hadOriginal = std::filesystem::exists(path, ec) && !ec;
    if (hadOriginal) {
        std::filesystem::remove(backup, ec);
        ec.clear();
        std::filesystem::rename(path, backup, ec);
        if (ec) {
            std::filesystem::remove(temporary, ec);
            return false;
        }
    }
    ec.clear();
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        if (hadOriginal) {
            std::error_code restoreEc;
            std::filesystem::rename(backup, path, restoreEc);
        }
        std::filesystem::remove(temporary, ec);
        return false;
    }
    return true;
}

} // namespace cakery
