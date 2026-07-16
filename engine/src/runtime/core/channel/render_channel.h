// do@Redlive

#pragma once

#include "dopch.h"

#include "base_channel.h"
#include "camera_channel.h"

namespace dodoe {

    using MainCameraData = CameraData;
    using MainCameraChannel = DataChannel<MainCameraData>;

    inline MainCameraChannel& GetMainCameraChannel() {
        static MainCameraChannel channel;
        return channel;
    }

} // namespace dodoe