// do@Redlive

#include "entity_res.h"

namespace dodoe {

    template<>
    Json Serializer::write(const EntityRes& instance) {
        Json json = Json::object();
        json["m_uuid"] = Serializer::write(instance.m_uuid);
        json["m_name"] = Serializer::write(instance.m_name);
        json["m_native_components"] = Json::array();
        for (const auto& component : instance.m_native_components) {
            json["m_native_components"].push_back(Serializer::write(component));
        }
        json["m_mono_components"] = Json::array();
        for (const auto& component : instance.m_mono_components) {
            json["m_mono_components"].push_back(Serializer::write(component));
        }
        return json;
    }

    template<>
    EntityRes& Serializer::read(const Json& json_context, EntityRes& instance) {
        if (!json_context.is_object()) {
            return instance;
        }

        if (json_context.contains("m_uuid")) {
            Serializer::read(json_context.at("m_uuid"), instance.m_uuid);
        }
        if (json_context.contains("m_name")) {
            Serializer::read(json_context.at("m_name"), instance.m_name);
        }

        instance.m_native_components.clear();
        if (json_context.contains("m_native_components") && json_context.at("m_native_components").is_array()) {
            for (const auto& component_json : json_context.at("m_native_components")) {
                ComponentRes component;
                Serializer::read(component_json, component);
                instance.m_native_components.push_back(std::move(component));
            }
        }

        instance.m_mono_components.clear();
        if (json_context.contains("m_mono_components") && json_context.at("m_mono_components").is_array()) {
            for (const auto& component_json : json_context.at("m_mono_components")) {
                ComponentRes component;
                Serializer::read(component_json, component);
                instance.m_mono_components.push_back(std::move(component));
            }
        }

        return instance;
    }

} // dodoe
