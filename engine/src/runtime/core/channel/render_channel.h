// do@Redlive

#include "dopch.h"

#include "base_channel.h"

namespace dodoe {

    struct MainCameraData {
        Matrix4f view{1.0f};
        Matrix4f projection{1.0f};
    };

    using MainCameraChannel = DataChannel<MainCameraData>;

    inline MainCameraChannel& GetMainCameraChannel() {
        static MainCameraChannel channel;
        return channel;
    }

} // dodoe