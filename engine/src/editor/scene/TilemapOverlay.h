// do@Redlive
#pragma once
#include "runtime/core/channel/gizmo_channel.h"

namespace dodoe { class Entity; }
namespace cakery {
class TilePaintService;
void drawTilemapOverlay(dodoe::GizmoChannelData& data, const TilePaintService& paint, dodoe::Entity tilemap);
}
