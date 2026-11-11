// do@Redlive
#include "dopch.h"
#include "SceneGrid.h"
#include "backend/EditorCamera.h"
#include <algorithm>
#include <cmath>

namespace cakery {

void configureSceneGrid(const EditorCamera& camera, dodoe::GizmoGridData& grid)
{
    grid = dodoe::GizmoGridData{};
    grid.ortho2d = camera.mode() == EditorCamera::Mode::Ortho2D;
    const float target = std::max(camera.pixelsToWorld(camera.pivot(), 32.0f), 0.0001f);
    const float decade = std::pow(10.0f, std::floor(std::log10(target)));
    const float fraction = target / decade;
    grid.minor_spacing = decade * (fraction <= 1.0f ? 1.0f : fraction <= 2.0f ? 2.0f : fraction <= 5.0f ? 5.0f : 10.0f);
    grid.major_spacing = grid.minor_spacing * 5.0f;
    if (grid.ortho2d) {
        // Distinct, representable floats: 1e8f + 1 rounds back to 1e8f and
        // makes smoothstep's interval degenerate, hiding the entire grid.
        grid.fade_begin = 1000000.0f;
        grid.fade_end = 2000000.0f;
    } else {
        grid.fade_begin = std::max(40.0f, camera.distance() * 2.0f);
        grid.fade_end = std::max(300.0f, grid.fade_begin * 4.0f);
    }
}

} // namespace cakery
