// do@Redlive

#include "camera_channel.h"

namespace dodoe {

    EditorCameraChannel& GetEditorCameraChannel() {
        static EditorCameraChannel channel;
        return channel;
    }

} // namespace dodoe
