// do@Redlive

#include "dopch.h"

#include "TilemapDocumentRefs.h"

#include "core/document/EditorDocumentModel.h"

namespace cakery {

Json* FindTilemapComponentValue(EditorDocumentModel& model, dodoe::UUID tilemap)
{
    EditorEntity* entity = model.findEntity(static_cast<UInt64>(tilemap));
    if (!entity) return nullptr;
    for (auto& component : entity->nativeComponents) {
        if (component.typeName == "TilemapComponent") {
            return &component.value;
        }
    }
    return nullptr;
}

Json* FindTilemapTilesetsArray(EditorDocumentModel& model, dodoe::UUID tilemap)
{
    Json* value = FindTilemapComponentValue(model, tilemap);
    if (!value) return nullptr;
    if (!value->contains("tilesets") || !(*value)["tilesets"].is_array()) {
        (*value)["tilesets"] = Json::array();
    }
    return &(*value)["tilesets"];
}

Json* FindTilemapLayerOrderArray(EditorDocumentModel& model, dodoe::UUID tilemap)
{
    Json* value = FindTilemapComponentValue(model, tilemap);
    if (!value) return nullptr;
    if (!value->contains("layer_order") || !(*value)["layer_order"].is_array()) {
        (*value)["layer_order"] = Json::array();
    }
    return &(*value)["layer_order"];
}

} // namespace cakery
