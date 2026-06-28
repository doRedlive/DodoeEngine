// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/mesh_draw/view_mesh_draw_context.h"

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
        ViewMeshVisibilityData m_visibility_data{};
        ViewMeshInstanceData m_instance_data{};
        ViewMeshPassData m_pass_data{};
        ViewMeshShaderData m_shader_data{};

    public:
        RenderView() = default;
        explicit RenderView(const Identifier id) : m_id(id) { }

        void setViewportRect(const Vector4i& rect) { m_viewport_rect = rect; }
        void setMatrices(const Matrix4f& view_matrix, const Matrix4f& projection_matrix);
        void buildFromViewInfo(const ViewInfo& info);
        void resetFrameData();

        void buildVisiblePrimitives(const RenderScene& scene);

        [[nodiscard]] Identifier getId() const { return m_id; }
        [[nodiscard]] const Vector4i& getViewportRect() const { return m_viewport_rect; }
        [[nodiscard]] const Matrix4f& getViewMatrix() const { return m_view_matrix; }
        [[nodiscard]] const Matrix4f& getProjectionMatrix() const { return m_projection_matrix; }
        [[nodiscard]] const Matrix4f& getViewProjectionMatrix() const { return m_view_projection_matrix; }

        [[nodiscard]] ViewMeshVisibilityData& getVisibilityData() { return m_visibility_data; }
        [[nodiscard]] const ViewMeshVisibilityData& getVisibilityData() const { return m_visibility_data; }
        [[nodiscard]] ViewMeshInstanceData& getInstanceData() { return m_instance_data; }
        [[nodiscard]] const ViewMeshInstanceData& getInstanceData() const { return m_instance_data; }
        [[nodiscard]] ViewMeshPassData& getPassData() { return m_pass_data; }
        [[nodiscard]] const ViewMeshPassData& getPassData() const { return m_pass_data; }
        [[nodiscard]] ViewMeshShaderData& getShaderData() { return m_shader_data; }
        [[nodiscard]] const ViewMeshShaderData& getShaderData() const { return m_shader_data; }
    };

} // dodoe
