// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/meta/serializer/serializer.h"

REFLECTION_TYPE(ComponentRes)

namespace dodoe {

    CLASS(ComponentRes, Fields) {
        REFLECTION_BODY(ComponentRes);

    public:
        META(Enable)
        String m_type_name;
        META(Enable)
        String m_component;
    };

    template<>
    Json Serializer::write(const ComponentRes& instance);
    template<>
    ComponentRes& Serializer::read(const Json& json_context, ComponentRes& instance);

} // dodoe
