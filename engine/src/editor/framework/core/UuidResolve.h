// do@Redlive

#pragma once

#include "runtime/core/utils/uuid.h"

namespace dodoe {
    class Scene;
    class Entity;
}

namespace cakery {

dodoe::Entity ResolveEntity(dodoe::Scene* scene, dodoe::UUID uuid);

} // namespace cakery
