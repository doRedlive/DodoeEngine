// do@Redlive
#pragma once
#include "runtime/core/math/math.h"

namespace dodoe { class Entity; }
namespace cakery {

struct SceneSelectionBounds {
    dodoe::Matrix4f localToWorld{1.0f};
    dodoe::Vector3f minimum{0.0f};
    dodoe::Vector3f maximum{0.0f};
    bool planar = false;
    bool hasGeometry = false;
};

dodoe::Matrix4f sceneEntityParentTransform(dodoe::Entity entity);
dodoe::Matrix4f sceneEntityTransform(dodoe::Entity entity);
SceneSelectionBounds sceneSelectionBounds(dodoe::Entity entity);

} // namespace cakery
