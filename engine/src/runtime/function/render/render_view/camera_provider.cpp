// do@Redlive

#include "camera_provider.h"

#include "runtime/core/channel/camera_channel.h"

namespace dodoe {

    Matrix4f IndexedCameraProvider::getView() const {
        auto& registry = GetCameraRegistry();
        if (m_index < registry.active_count) {
            return registry.cameras[m_index].view;
        }
        return Matrix4f(1.0f);
    }

    Matrix4f IndexedCameraProvider::getProj() const {
        auto& registry = GetCameraRegistry();
        if (m_index < registry.active_count) {
            return registry.cameras[m_index].projection;
        }
        return Matrix4f(1.0f);
    }

#ifdef DODOE_EDITOR_ENABLED
    Matrix4f EditorCameraProvider::getView() const {
        return GetEditorCameraChannel().get<CameraData>().view;
    }

    Matrix4f EditorCameraProvider::getProj() const {
        return GetEditorCameraChannel().get<CameraData>().projection;
    }
#endif

} // namespace dodoe
