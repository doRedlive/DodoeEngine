// do@Redlive

#include "EditorDocumentSerializer.h"

#include <fstream>
#include <utility>

namespace cakery {

namespace {

std::vector<EditorComponent> ReadComponents(const nlohmann::json& entityJson, const char* key) {
    std::vector<EditorComponent> components;
    if (!entityJson.contains(key) || !entityJson[key].is_array()) {
        return components;
    }
    for (const auto& item : entityJson[key]) {
        EditorComponent component;
        if (item.contains("m_type_name") && item["m_type_name"].is_string()) {
            component.typeName = item["m_type_name"].get<std::string>();
        }
        if (item.contains("m_component") && item["m_component"].is_string()) {
            try {
                component.value = nlohmann::json::parse(item["m_component"].get<std::string>());
            } catch (const nlohmann::json::exception&) {
                component.value = nlohmann::json::object();
            }
        }
        components.push_back(std::move(component));
    }
    return components;
}

std::uint64_t ParentFromHierarchyComponent(const std::vector<EditorComponent>& components)
{
    for (const auto& component : components) {
        if (component.typeName != "HierarchyComponent" || !component.value.is_object()) {
            continue;
        }
        const auto it = component.value.find("parent_uuid");
        if (it != component.value.end() &&
            (it->is_number_unsigned() || it->is_number_integer())) {
            return it->get<std::uint64_t>();
        }
    }
    return 0;
}

nlohmann::json WriteComponents(const std::vector<EditorComponent>& components) {
    nlohmann::json array = nlohmann::json::array();
    for (const auto& component : components) {
        nlohmann::json item;
        item["m_type_name"] = component.typeName;
        item["m_component"] = component.value.dump();
        array.push_back(std::move(item));
    }
    return array;
}

} // namespace

nlohmann::json EditorDocumentSerializer::toJson(const EditorDocument& document)
{
    nlohmann::json root;
    root["m_name"] = document.name;

    nlohmann::json entities = nlohmann::json::array();
    for (const auto& entity : document.entities) {
        nlohmann::json entityJson;
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

bool EditorDocumentSerializer::fromJson(const nlohmann::json& root, EditorDocument& outDocument)
{
    EditorDocument document;
    if (root.contains("m_name") && root["m_name"].is_string()) {
        document.name = root["m_name"].get<std::string>();
    }
    if (root.contains("m_entities") && root["m_entities"].is_array()) {
        for (const auto& entityJson : root["m_entities"]) {
            EditorEntity entity;
            if (entityJson.contains("m_uuid") && entityJson["m_uuid"].is_number_integer()) {
                entity.uuid = entityJson["m_uuid"].get<std::uint64_t>();
            }
            const bool hasSerializedParent = entityJson.contains("m_parent") &&
                (entityJson["m_parent"].is_number_unsigned() || entityJson["m_parent"].is_number_integer());
            if (hasSerializedParent) {
                entity.parent = entityJson["m_parent"].get<std::uint64_t>();
            }
            if (entityJson.contains("m_name") && entityJson["m_name"].is_string()) {
                entity.name = entityJson["m_name"].get<std::string>();
            }
            entity.nativeComponents = ReadComponents(entityJson, "m_native_components");
            entity.managedComponents = ReadComponents(entityJson, "m_managed_components");
            if (!hasSerializedParent || entity.parent == 0) {
                entity.parent = ParentFromHierarchyComponent(entity.nativeComponents);
            }
            document.entities.push_back(std::move(entity));
        }
    }

    outDocument = std::move(document);
    return true;
}

bool EditorDocumentSerializer::load(const std::filesystem::path& path, EditorDocument& outDocument) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    nlohmann::json root;
    try {
        file >> root;
    } catch (const nlohmann::json::exception&) {
        return false;
    }

    return fromJson(root, outDocument);
}

bool EditorDocumentSerializer::save(const EditorDocument& document, const std::filesystem::path& path) {
    nlohmann::json root = toJson(document);

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << root.dump(4) << std::endl;
    return true;
}

} // namespace cakery
