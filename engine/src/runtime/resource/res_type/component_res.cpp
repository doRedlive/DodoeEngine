// do@Redlive

#include "component_res.h"

namespace dodoe {

    template<>
    Json Serializer::write(const ComponentRes& instance) {
        Json json = Json::object();
        json["m_type_name"] = Serializer::write(instance.m_type_name);
        json["m_component"] = Serializer::write(instance.m_component);
        return json;
    }

    template<>
    ComponentRes& Serializer::read(const Json& json_context, ComponentRes& instance) {
        if (json_context.is_object()) {
            if (json_context.contains("m_type_name")) {
                Serializer::read(json_context.at("m_type_name"), instance.m_type_name);
            }
            if (json_context.contains("m_component")) {
                Serializer::read(json_context.at("m_component"), instance.m_component);
            }
        }
        return instance;
    }

} // dodoe
