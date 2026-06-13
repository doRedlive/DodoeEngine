// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/mesh_draw/view_mesh_draw_context.h"

namespace dodoe {

    class RenderView {
        Identifier m_id{};
        Vector4i m_viewport_rect{0, 0, 0, 0};
        Matrix4f m_view_matrix{1.0f};
        Matrix4f m_projection_matrix{1.0f};
        Matrix4f m_view_projection_matrix{1.0f};
        ViewMeshDrawContext m_mesh_draw_context{};

    public:
        RenderView() = default;
        explicit RenderView(const Identifier id) : m_id(id) { }

        void setViewportRect(const Vector4i& viewport_rect) { m_viewport_rect = viewport_rect; }
        void setMatrices(const Matrix4f& view_matrix, const Matrix4f& projection_matrix) {
            m_view_matrix = view_matrix;
            m_projection_matrix = projection_matrix;
            m_view_projection_matrix = projection_matrix * view_matrix;
        }
        void resetFrameData() { m_mesh_draw_context.reset(); }

        [[nodiscard]] Identifier getId() const { return m_id; }
        [[nodiscard]] const Vector4i& getViewportRect() const { return m_viewport_rect; }
        [[nodiscard]] const Matrix4f& getViewMatrix() const { return m_view_matrix; }
        [[nodiscard]] const Matrix4f& getProjectionMatrix() const { return m_projection_matrix; }
        [[nodiscard]] const Matrix4f& getViewProjectionMatrix() const { return m_view_projection_matrix; }
        [[nodiscard]] ViewMeshDrawContext& getMeshDrawContext() { return m_mesh_draw_context; }
        [[nodiscard]] const ViewMeshDrawContext& getMeshDrawContext() const { return m_mesh_draw_context; }

    };

} // dodoe
