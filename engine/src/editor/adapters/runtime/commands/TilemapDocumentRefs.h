// do@Redlive

#pragma once

#include "runtime/core/utils/uuid.h"

#include <nlohmann/json.hpp>

namespace cakery {

class EditorDocumentModel;

nlohmann::json* FindTilemapComponentValue(EditorDocumentModel& model, dodoe::UUID tilemap);
nlohmann::json* FindTilemapTilesetsArray(EditorDocumentModel& model, dodoe::UUID tilemap);
nlohmann::json* FindTilemapLayerOrderArray(EditorDocumentModel& model, dodoe::UUID tilemap);

} // namespace cakery
