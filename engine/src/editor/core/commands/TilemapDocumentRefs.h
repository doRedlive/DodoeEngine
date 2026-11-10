// do@Redlive
#pragma once
#include "dopch.h"

#include "runtime/core/utils/uuid.h"


namespace cakery {

class EditorDocumentModel;

Json* FindTilemapComponentValue(EditorDocumentModel& model, dodoe::UUID tilemap);
Json* FindTilemapTilesetsArray(EditorDocumentModel& model, dodoe::UUID tilemap);
Json* FindTilemapLayerOrderArray(EditorDocumentModel& model, dodoe::UUID tilemap);

} // namespace cakery
