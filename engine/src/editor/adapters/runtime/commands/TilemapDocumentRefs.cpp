// do@Redlive

#include "TilemapDocumentRefs.h"

#include "core/document/EditorDocumentModel.h"

namespace cakery {

nlohmann::json* FindTilemapComponentValue(EditorDocumentModel& model, dodoe::UUID tilemap)
{
    EditorEntity* entity = model.findEntity(static_cast<std::uint64_t>(tilemap));
    if (!entity) return nullptr;
    for (auto& component : entity->nativeComponents) {
        if (component.typeName == "TilemapComponent") {
            return &component.value;
        }
    }
    return nullptr;
}

nlohmann::json* FindTilemapTilesetsArray(EditorDocumentModel& model, dodoe::UUID tilemap)
{
    nlohmann::json* value = FindTilemapComponentValue(model, tilemap);
    if (!value) return nullptr;
    if (!value->contains("tilesets") || !(*value)["tilesets"].is_array()) {
        (*value)["tilesets"] = nlohmann::json::array();
    }
    return &(*value)["tilesets"];
}

nlohmann::json* FindTilemapLayerOrderArray(EditorDocumentModel& model, dodoe::UUID tilemap)
{
    nlohmann::json* value = FindTilemapComponentValue(model, tilemap);
    if (!value) return nullptr;
    if (!value->contains("layer_order") || !(*value)["layer_order"].is_array()) {
        (*value)["layer_order"] = nlohmann::json::array();
    }
    return &(*value)["layer_order"];
}

} // namespace cakery
