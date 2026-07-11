// do@Redlive

#pragma once

#include "runtime/core/math/math.h"

namespace dodoe {
    struct TilemapComponent;
}

namespace cakery {

class TileCoord {
public:
    static bool worldToCell(const dodoe::TilemapComponent& tm,
                            const dodoe::Matrix4f& mapWorld,
                            const dodoe::Vector3f& worldPos,
                            int& outX, int& outY);

    static dodoe::Vector3f cellToWorld(const dodoe::TilemapComponent& tm,
                                        const dodoe::Matrix4f& mapWorld,
                                        int x, int y);
};

} // namespace cakery
