// do@Redlive

#include "physics_debug.h"

#include <cfloat>
#include <glm/gtx/quaternion.hpp>

#include "runtime/function/render/render_command_queue.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"

namespace dodoe {

    namespace {

        constexpr Size_t kBoxVertexCount = 8;
        constexpr Size_t kBoxIndexCount = 36;

        Vector4f ColorToVector4(const UInt32 rgba) {
            return {
                static_cast<float>((rgba >> 24) & 0xFF) / 255.0f,
                static_cast<float>((rgba >> 16) & 0xFF) / 255.0f,
                static_cast<float>((rgba >> 8) & 0xFF) / 255.0f,
                static_cast<float>(rgba & 0xFF) / 255.0f,
            };
        }

    }

    bool PhysicsDebugger::initialize(const PhysicsDebuggerCreateInfo& create_info) {
        m_line_thickness = std::max(create_info.line_thickness, 0.1f);
        m_point_size = std::max(create_info.point_size, 0.1f);
        return true;
    }

    void PhysicsDebugger::shutdown() {
        for (const UUID& uuid : m_submitted) {
            RenderCommandQueue::RemovePrimitive(uuid);
        }
        m_submitted.clear();
        m_lines.clear();
        m_points.clear();
        m_line_thickness = 2.0f;
        m_point_size = 4.0f;
    }

    void PhysicsDebugger::drawLine(const Vector3f& start, const Vector3f& end, const UInt32 color) {
        DebugLine line;
        line.start = start;
        line.end = end;
        line.color = color;
        m_lines.push_back(line);
    }

    void PhysicsDebugger::drawPoint(const Vector3f& position, const UInt32 color) {
        DebugPoint point;
        point.position = position;
        point.color = color;
        m_points.push_back(point);
    }

    void PhysicsDebugger::flush() {
        for (const UUID& uuid : m_submitted) {
            RenderCommandQueue::RemovePrimitive(uuid);
        }
        m_submitted.clear();

        for (const DebugLine& line : m_lines) {
            const UUID uuid = UUID();
            auto object = buildLineObject(line);
            object->setUUID(uuid);
            RenderCommandQueue::AddPrimitive(std::move(object));
            m_submitted.push_back(uuid);
        }

        for (const DebugPoint& point : m_points) {
            const UUID uuid = UUID();
            auto object = buildPointObject(point);
            object->setUUID(uuid);
            RenderCommandQueue::AddPrimitive(std::move(object));
            m_submitted.push_back(uuid);
        }

        m_lines.clear();
        m_points.clear();
    }

    Scope<PrimitiveRenderObject> PhysicsDebugger::buildLineObject(const DebugLine& line) const {
        auto object = create_scope<PrimitiveRenderObject>();
        object->setMobility(PrimitiveMobility::Movable);
        object->setVisible(true);
        object->setCastShadow(false);
        object->setWorldTransform(buildLineMatrix(line));

        MeshUploadData upload_data;
        upload_data.name = "physics_debug_line";
        upload_data.position_data = {
            { -0.5f, -0.5f, -0.5f }, { 0.5f, -0.5f, -0.5f }, { 0.5f, 0.5f, -0.5f }, { -0.5f, 0.5f, -0.5f },
            { -0.5f, -0.5f, 0.5f }, { 0.5f, -0.5f, 0.5f }, { 0.5f, 0.5f, 0.5f }, { -0.5f, 0.5f, 0.5f },
        };
        upload_data.texcoord_data.assign(kBoxVertexCount, Vector2f(0.0f));
        upload_data.normal_data.assign(kBoxVertexCount, 0u);
        upload_data.index_data = {
            0, 1, 2, 2, 3, 0,
            1, 5, 6, 6, 2, 1,
            5, 4, 7, 7, 6, 5,
            4, 0, 3, 3, 7, 4,
            3, 2, 6, 6, 7, 3,
            4, 5, 1, 1, 0, 4,
        };
        object->setUploadData(upload_data);

        SubMesh section;
        section.material.color = ColorToVector4(line.color);
        section.index_offset = 0;
        section.vertex_offset = 0;
        section.index_count = static_cast<UInt32>(kBoxIndexCount);
        section.vertex_count = static_cast<UInt32>(kBoxVertexCount);
        section.section_index = 0;
        section.primitive_type = MeshGeometryPrimitiveType::Triangles;

        MeshLODData lod;
        lod.screen_size = 1.0f;
        lod.sub_meshes.push_back(section);

        DynamicArray<MeshLODData> lods;
        lods.push_back(std::move(lod));
        object->setLODData(lods);
        return object;
    }

    Scope<PrimitiveRenderObject> PhysicsDebugger::buildPointObject(const DebugPoint& point) const {
        auto object = buildLineObject(DebugLine{ point.position, point.position, point.color });

        Matrix4f world(1.0f);
        world = Math::Scale(world, Vector3f(m_point_size, m_point_size, m_point_size));
        world = Math::Translate(world, point.position);
        object->setWorldTransform(world);
        return object;
    }

    Matrix4f PhysicsDebugger::buildLineMatrix(const DebugLine& line) const {
        const Vector3f delta = line.end - line.start;
        const float length = glm::length(delta);
        if (length <= FLT_EPSILON) {
            return Matrix4f(1.0f);
        }

        const Vector3f direction = delta / length;
        const Vector3f mid = (line.start + line.end) * 0.5f;

        const Quaternion orientation = glm::rotation(Vector3f(1.0f, 0.0f, 0.0f), direction);
        Matrix4f world(1.0f);
        world = Math::Scale(world, Vector3f(length, m_line_thickness, m_line_thickness));
        world = glm::toMat4(orientation) * world;
        world = Math::Translate(world, mid);
        return world;
    }

} // dodoe
