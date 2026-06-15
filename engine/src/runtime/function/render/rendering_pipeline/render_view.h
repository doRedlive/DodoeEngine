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
        ViewMeshVisibilityData m_visibility_data{};
        ViewMeshInstanceData m_instance_data{};
        ViewMeshPassData m_pass_data{};
        ViewMeshShaderData m_shader_data{};

    public:
        RenderView() = default;
        explicit RenderView(const Identifier id) : m_id(id) { }

        void setViewportRect(const Vector4i& viewport_rect) { m_viewport_rect = viewport_rect; }
        void setMatrices(const Matrix4f& view_matrix, const Matrix4f& projection_matrix) {
            m_view_matrix = view_matrix;
            m_projection_matrix = projection_matrix;
            m_view_projection_matrix = projection_matrix * view_matrix;
        }
        void resetFrameData() {
            m_visibility_data.reset();
            m_instance_data.reset();
            m_pass_data.reset();
            m_shader_data.reset();
        }

        [[nodiscard]] Identifier getId() const { return m_id; }
        [[nodiscard]] const Vector4i& getViewportRect() const { return m_viewport_rect; }
        [[nodiscard]] const Matrix4f& getViewMatrix() const { return m_view_matrix; }
        [[nodiscard]] const Matrix4f& getProjectionMatrix() const { return m_projection_matrix; }
        [[nodiscard]] const Matrix4f& getViewProjectionMatrix() const { return m_view_projection_matrix; }

        [[nodiscard]] ViewMeshVisibilityData& visibilityData() { return m_visibility_data; }
        [[nodiscard]] const ViewMeshVisibilityData& visibilityData() const { return m_visibility_data; }
        [[nodiscard]] ViewMeshInstanceData& instanceData() { return m_instance_data; }
        [[nodiscard]] const ViewMeshInstanceData& instanceData() const { return m_instance_data; }
        [[nodiscard]] ViewMeshPassData& passData() { return m_pass_data; }
        [[nodiscard]] const ViewMeshPassData& passData() const { return m_pass_data; }
        [[nodiscard]] ViewMeshShaderData& shaderData() { return m_shader_data; }
        [[nodiscard]] const ViewMeshShaderData& shaderData() const { return m_shader_data; }

    };

} // dodoe
