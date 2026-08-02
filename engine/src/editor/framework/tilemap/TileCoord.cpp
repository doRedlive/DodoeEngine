// do@Redlive

#include "TileCoord.h"

#include "runtime/function/world/components/tilemap/tilemap_component.h"

namespace cakery {

bool TileCoord::worldToCell(const dodoe::TilemapComponent& tm,
                             const dodoe::Matrix4f& mapWorld,
                             const dodoe::Vector3f& worldPos,
                             int& outX, int& outY)
{
    dodoe::Vector3f localPos = dodoe::Vector3f(glm::inverse(mapWorld) * dodoe::Vector4f(worldPos, 1.0f));

    float cellX = localPos.x / static_cast<float>(tm.tile_width);
    float cellY = localPos.y / static_cast<float>(tm.tile_height);

    outX = static_cast<int>(std::floor(cellX));
    outY = static_cast<int>(std::floor(cellY));

    if (outX < 0 || outY < 0 ||
        static_cast<dodoe::UInt32>(outX) >= tm.map_width ||
        static_cast<dodoe::UInt32>(outY) >= tm.map_height) {
        return false;
    }

    return true;
}

dodoe::Vector3f TileCoord::cellToWorld(const dodoe::TilemapComponent& tm,
                                        const dodoe::Matrix4f& mapWorld,
                                        int x, int y)
{
    dodoe::Vector3f localPos{
        (static_cast<float>(x) + 0.5f) * static_cast<float>(tm.tile_width),
        (static_cast<float>(y) + 0.5f) * static_cast<float>(tm.tile_height),
        0.0f
    };

    return dodoe::Vector3f(mapWorld * dodoe::Vector4f(localPos, 1.0f));
}

} // namespace cakery
