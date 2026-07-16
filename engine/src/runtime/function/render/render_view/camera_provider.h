// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    struct ICameraProvider {
        virtual ~ICameraProvider() = default;
        virtual Matrix4f getView() const = 0;
        virtual Matrix4f getProj() const = 0;
#ifdef DODOE_EDITOR_ENABLED
        [[nodiscard]] virtual Bool isEditorCamera() const { return false; }
#endif
    };

    class IndexedCameraProvider : public ICameraProvider {
        Size_t m_index;
    public:
        explicit IndexedCameraProvider(Size_t index) : m_index(index) {}
        Matrix4f getView() const override;
        Matrix4f getProj() const override;
    };

#ifdef DODOE_EDITOR_ENABLED
    class EditorCameraProvider : public ICameraProvider {
    public:
        Matrix4f getView() const override;
        Matrix4f getProj() const override;
        [[nodiscard]] Bool isEditorCamera() const override { return true; }
    };
#endif

} // namespace dodoe
