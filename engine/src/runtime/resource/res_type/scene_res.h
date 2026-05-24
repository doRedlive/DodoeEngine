// do@Redlive

#pragma once

#include "dopch.h"

#include "entity_res.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/meta/serializer/serializer.h"

REFLECTION_TYPE(SceneRes)

namespace dodoe {
    class Scene;

    CLASS(SceneRes, Fields) {
        REFLECTION_BODY(SceneRes);

    public:
        META(Enable)
        std::string m_name;
        META(Enable)
        std::vector<EntityRes> m_entities;
    };

    template<>
    Json Serializer::write(const SceneRes& instance);
    template<>
    SceneRes& Serializer::read(const Json& json_context, SceneRes& instance);
} // dodoe
