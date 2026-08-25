// do@Redlive

#include "ReorderTileLayerCommand.h"

#include "TilemapDocumentRefs.h"
#include "adapters/runtime/services/UuidResolve.h"
#include "core/document/EditorDocumentModel.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/scene.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace cakery {

namespace {

dodoe::Scene* ActiveScene() {
    dodoe::World* world = dodoe::GetWorld();
    return world ? world->getActiveScene() : nullptr;
}

int FindChildIndex(dodoe::HierarchyComponent& hierarchy, dodoe::UUID layer)
{
    for (std::size_t i = 0; i < hierarchy.children.size(); ++i) {
        if (hierarchy.children[i].valid() && hierarchy.children[i].uuid() == layer) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace

ReorderTileLayerCommand::ReorderTileLayerCommand(dodoe::UUID tilemap, dodoe::UUID layer, bool moveUp)
    : m_tilemap(tilemap)
    , m_layer(layer)
    , m_moveUp(moveUp)
{}

void ReorderTileLayerCommand::swapInScene(EditorDocumentModel& model, bool reverse)
{
    auto* scene = ActiveScene();
    if (!scene) return;

    bool up = reverse ? !m_moveUp : m_moveUp;
    auto tilemapEntity = ResolveEntity(scene, m_tilemap);
    if (!tilemapEntity.valid() || !tilemapEntity.hasComponent<dodoe::HierarchyComponent>()) return;
    auto& hierarchy = tilemapEntity.getComponent<dodoe::HierarchyComponent>();

    const int index = FindChildIndex(hierarchy, m_layer);
    if (index < 0) return;
    const int other = up ? index - 1 : index + 1;
    if (other < 0 || other >= static_cast<int>(hierarchy.children.size())) return;

    auto& children = hierarchy.children;
    dodoe::Entity otherEntity = children[static_cast<std::size_t>(other)];
    if (otherEntity.valid()) {
        m_otherLayer = otherEntity.uuid();
    }
    std::swap(children[static_cast<std::size_t>(index)],
              children[static_cast<std::size_t>(other)]);
    hierarchy.child_count = static_cast<int>(children.size());
    hierarchy.dirty = true;

    if (nlohmann::json* order = FindTilemapLayerOrderArray(model, m_tilemap)) {
        if (order->is_array()) {
            const std::uint64_t layerId = static_cast<std::uint64_t>(m_layer);
            const std::uint64_t otherId = static_cast<std::uint64_t>(m_otherLayer);
            const auto findItem = [layerId](const nlohmann::json& item) {
                return item.is_number_unsigned() && item.get<std::uint64_t>() == layerId;
            };
            auto layerIt = std::find_if(order->begin(), order->end(), findItem);
            auto otherIt = std::find_if(order->begin(), order->end(),
                                        [otherId](const nlohmann::json& item) {
                                            return item.is_number_unsigned() &&
                                                   item.get<std::uint64_t>() == otherId;
                                        });
            if (layerIt != order->end() && otherIt != order->end()) {
                std::iter_swap(layerIt, otherIt);
            }
        }
    }
}

void ReorderTileLayerCommand::execute(EditorDocumentModel& model)
{
    swapInScene(model, false);
    m_swapped = true;
}

void ReorderTileLayerCommand::revert(EditorDocumentModel& model)
{
    if (!m_swapped) return;
    swapInScene(model, true);
}

std::string ReorderTileLayerCommand::label() const
{
    return m_moveUp ? std::string("Move Layer Up") : std::string("Move Layer Down");
}

} // namespace cakery
