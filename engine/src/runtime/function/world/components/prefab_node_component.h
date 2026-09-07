// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/uuid.h"

namespace dodoe {

    struct PrefabNodeComponent {
        UUID template_uuid{};

        PrefabNodeComponent() = default;
        explicit PrefabNodeComponent(UUID in_uuid) : template_uuid(in_uuid) {}
    };

} // dodoe
