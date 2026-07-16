// do@Redlive

#pragma once

#include "dopch.h"

#include "base_channel.h"

namespace dodoe {

    static constexpr Size_t kMaxCameras = 4;

    struct CameraData {
        Matrix4f view{1.0f};
        Matrix4f projection{1.0f};
    };

    struct CameraChannelArray {
        CameraData cameras[kMaxCameras];
        UInt32 active_count{0};
    };

    inline CameraChannelArray& GetCameraRegistry() {
        static CameraChannelArray instance;
        return instance;
    }

    using EditorCameraChannel = DataChannel<CameraData>;

    inline EditorCameraChannel& GetEditorCameraChannel() {
        static EditorCameraChannel channel;
        return channel;
    }

} // namespace dodoe
