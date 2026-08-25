// do@Redlive

#include "gizmo_channel.h"

#ifdef DODOE_EDITOR_ENABLED

namespace dodoe {

    GizmoChannel& GetGizmoChannel() {
        static GizmoChannel channel;
        return channel;
    }

} // namespace dodoe

#endif // DODOE_EDITOR_ENABLED
