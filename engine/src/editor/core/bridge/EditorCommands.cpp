// do@Redlive

#include "dopch.h"

#include "EditorCommands.h"

namespace cakery {

const char* gizmoModeName(GizmoMode mode)
{
    switch (mode) {
    case GizmoMode::Translate: return "translate";
    case GizmoMode::Rotate: return "rotate";
    case GizmoMode::Scale: return "scale";
    case GizmoMode::None: break;
    }
    return "none";
}

GizmoMode gizmoModeFromName(StringView name)
{
    if (name == "translate") return GizmoMode::Translate;
    if (name == "rotate") return GizmoMode::Rotate;
    if (name == "scale") return GizmoMode::Scale;
    return GizmoMode::None;
}

} // namespace cakery
