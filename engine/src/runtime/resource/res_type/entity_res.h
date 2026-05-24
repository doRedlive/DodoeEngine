// do@Redlive

#pragma once

#include "dopch.h"

#include "component_res.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/meta/serializer/serializer.h"
#include "runtime/core/utils/uuid.h"

REFLECTION_TYPE(EntityRes)

namespace dodoe {

    CLASS(EntityRes, Fields) {
        REFLECTION_BODY(EntityRes);

    public:
        META(Enable)
        Uuid m_uuid{};
        META(Enable)
        std::string m_name;
        META(Enable)
        std::vector<ComponentRes> m_native_components;
        META(Enable)
        std::vector<ComponentRes> m_mono_components;
    };

    template<>
    Json Serializer::write(const EntityRes& instance);
    template<>
    EntityRes& Serializer::read(const Json& json_context, EntityRes& instance);

} // dodoe
