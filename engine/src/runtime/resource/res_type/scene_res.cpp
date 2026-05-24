// do@Redlive

#include "scene_res.h"

namespace dodoe {

    template<>
    Json Serializer::write(const SceneRes& instance) {
        Json json = Json::object();
        json["m_name"] = Serializer::write(instance.m_name);
        json["m_entities"] = Json::array();
        for (const auto& entity : instance.m_entities) {
            json["m_entities"].push_back(Serializer::write(entity));
        }
        return json;
    }

    template<>
    SceneRes& Serializer::read(const Json& json_context, SceneRes& instance) {
        if (!json_context.is_object()) {
            return instance;
        }

        if (json_context.contains("m_name")) {
            Serializer::read(json_context.at("m_name"), instance.m_name);
        }
        instance.m_entities.clear();
        if (json_context.contains("m_entities") && json_context.at("m_entities").is_array()) {
            for (const auto& entity_json : json_context.at("m_entities")) {
                EntityRes entity;
                Serializer::read(entity_json, entity);
                instance.m_entities.push_back(std::move(entity));
            }
        }
        return instance;
    }

} // dodoe
