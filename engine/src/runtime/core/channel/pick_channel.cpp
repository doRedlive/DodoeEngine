// do@Redlive

#include "pick_channel.h"

#ifdef DODOE_EDITOR_ENABLED

namespace dodoe {

    PickChannel& GetPickChannel() {
        static PickChannel channel;
        return channel;
    }

} // namespace dodoe

#endif // DODOE_EDITOR_ENABLED
