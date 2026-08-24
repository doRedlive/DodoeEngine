// do@Redlive

#include "input_action_asset.h"

#include "runtime/function/input/input_serialization.h"

namespace dodoe {

    namespace {
        String valueTypeName(InputActionValueType type) {
            switch (type) {
            case InputActionValueType::Axis1D: return "Axis1D";
            case InputActionValueType::Axis2D: return "Axis2D";
            default: return "Button";
            }
        }
    }

    Bool InputActionAsset::loadFromSource(const String& absolute_source_path) {
        std::ifstream file(absolute_source_path.c_str());
        if (!file.is_open()) return false;
        Json json;
        try {
            std::stringstream buffer;
            buffer << file.rdbuf();
            json = Json::parse(buffer.str());
        } catch (const Json::exception&) {
            return false;
        }
        if (!json.contains("maps") || !json["maps"].is_array()) return false;
        m_guid = json.value("guid", String{});
        m_version = json.value("version", 1);
        m_maps.clear();
        for (const auto& map_json : json["maps"]) {
            InputActionAssetMap map;
            map.name = map_json.value("name", String{});
            map.priority = map_json.value("priority", 0);
            map.enabled = map_json.value("enabled", true);
            map.consume_input = map_json.value("consume", false);
            for (const auto& action_json : map_json.value("actions", Json::array())) {
                InputActionAssetAction action;
                action.name = action_json.value("name", String{});
                const String type = action_json.value("type", String("Button"));
                action.value_type = type == "Axis1D" ? InputActionValueType::Axis1D :
                    (type == "Axis2D" ? InputActionValueType::Axis2D : InputActionValueType::Button);
                for (const auto& binding_json : action_json.value("bindings", Json::array())) {
                    action.bindings.push_back(ParseInputBinding(binding_json));
                }
                map.actions.push_back(std::move(action));
            }
            m_maps.push_back(std::move(map));
        }
        m_meta.source_path = absolute_source_path;
        return true;
    }

    void InputActionAsset::unloadRuntime() { m_maps.clear(); }

    Bool InputActionAsset::saveToSource(const String& absolute_path) const {
        std::ofstream file(absolute_path.c_str());
        if (!file.is_open()) return false;
        Json root;
        if (!m_guid.empty()) root["guid"] = m_guid;
        root["version"] = m_version;
        root["maps"] = Json::array();
        for (const auto& map : m_maps) {
            Json map_json;
            map_json["name"] = map.name;
            map_json["priority"] = map.priority;
            map_json["enabled"] = map.enabled;
            map_json["consume"] = map.consume_input;
            map_json["actions"] = Json::array();
            for (const auto& action : map.actions) {
                Json action_json;
                action_json["name"] = action.name;
                action_json["type"] = valueTypeName(action.value_type);
                action_json["bindings"] = Json::array();
                for (const auto& binding : action.bindings) {
                    action_json["bindings"].push_back(InputBindingToJson(binding));
                }
                map_json["actions"].push_back(std::move(action_json));
            }
            root["maps"].push_back(std::move(map_json));
        }
        file << root.dump(4);
        return true;
    }

} // namespace dodoe
