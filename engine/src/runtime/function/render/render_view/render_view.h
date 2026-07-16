// do@Redlive

#pragma once

#include "dopch.h"

#include "view_extension.h"

namespace dodoe {

    class RenderScene;

    struct ViewInfo {
        Matrix4f view{1.0f};
        Matrix4f projection{1.0f};
        Matrix4f view_projection{1.0f};
        Vector3f position{0.0f};
        Vector4i viewport_rect{0, 0, 0, 0};
    };

    class RenderView {
        Identifier m_id{};
        Vector4i m_viewport_rect{0, 0, 0, 0};
        Matrix4f m_view_matrix{1.0f};
        Matrix4f m_projection_matrix{1.0f};
        Matrix4f m_view_projection_matrix{1.0f};
        ViewExtensionContainer m_extensions{};
        UInt8 m_view_flags{0};

    public:
        RenderView() = default;
        explicit RenderView(const Identifier id) : m_id(id) { }

        void setViewportRect(const Vector4i& rect) { m_viewport_rect = rect; }
        void setMatrices(const Matrix4f& view_matrix, const Matrix4f& projection_matrix);
        void buildFromViewInfo(const ViewInfo& info);

        void buildVisiblePrimitives(const RenderScene& scene);
        void buildVisibleSprites(const RenderScene& scene);

        static constexpr UInt8 kShowEditorPrimitives = 1 << 0;

        void setViewFlags(UInt8 flags) { m_view_flags = flags; }
        void enableViewFlag(UInt8 flag) { m_view_flags |= flag; }
        void disableViewFlag(UInt8 flag) { m_view_flags &= ~flag; }
        [[nodiscard]] Bool hasViewFlag(UInt8 flag) const { return (m_view_flags & flag) != 0; }

        [[nodiscard]] Identifier getId() const { return m_id; }
        [[nodiscard]] const Vector4i& getViewportRect() const { return m_viewport_rect; }
        [[nodiscard]] const Matrix4f& getViewMatrix() const { return m_view_matrix; }
        [[nodiscard]] const Matrix4f& getProjectionMatrix() const { return m_projection_matrix; }
        [[nodiscard]] const Matrix4f& getViewProjectionMatrix() const { return m_view_projection_matrix; }

        template<typename TExtension, typename... TArgs>
        TExtension& getOrCreateExtension(TArgs&&... args) {
            return m_extensions.getOrCreate<TExtension>(std::forward<TArgs>(args)...);
        }

        template<typename TExtension>
        TExtension* getExtension() {
            return m_extensions.get<TExtension>();
        }

        template<typename TExtension>
        const TExtension* getExtension() const {
            return m_extensions.get<TExtension>();
        }

        template<typename TExtension>
        Bool hasExtension() const {
            return m_extensions.has<TExtension>();
        }

        void resetExtensions() {
            m_extensions.reset();
        }
    };

} // dodoe
