// do@Redlive
#pragma once
#include "runtime/core/channel/gizmo_channel.h"

namespace cakery {
class EditorCamera;
// Grid presentation is independent of selection and transform-tool state.
void configureSceneGrid(const EditorCamera& camera, dodoe::GizmoGridData& grid);
}
