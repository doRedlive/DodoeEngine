// do@Redlive

#include "physics_debug.h"

#include <cfloat>
#include <cstddef>
#include <cstring>
#include <glm/gtx/quaternion.hpp>

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/mesh_draw/mesh.h"
#include "runtime/function/render/render_command_queue.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"

namespace dodoe {

    namespace {

        constexpr Size_t kBoxVertexCount = 8;
        constexpr Size_t kBoxIndexCount = 36;
        constexpr Size_t kBoxVertexStride = sizeof(Vector3f) + sizeof(UInt32) + sizeof(Vector2f);
        constexpr Size_t kBoxVertexByteSize = kBoxVertexStride * kBoxVertexCount;
        constexpr Size_t kBoxIndexByteSize = sizeof(UInt32) * kBoxIndexCount;

        Vector4f ColorToVector4(const UInt32 rgba) {
            return {
                static_cast<float>((rgba >> 24) & 0xFF) / 255.0f,
                static_cast<float>((rgba >> 16) & 0xFF) / 255.0f,
                static_cast<float>((rgba >> 8) & 0xFF) / 255.0f,
                static_cast<float>(rgba & 0xFF) / 255.0f,
            };
        }

        Scope<Mesh> BuildUnitBoxMesh() {
            const Vector3f positions[kBoxVertexCount] = {
                { -0.5f, -0.5f, -0.5f }, { 0.5f, -0.5f, -0.5f }, { 0.5f, 0.5f, -0.5f }, { -0.5f, 0.5f, -0.5f },
                { -0.5f, -0.5f, 0.5f }, { 0.5f, -0.5f, 0.5f }, { 0.5f, 0.5f, 0.5f }, { -0.5f, 0.5f, 0.5f },
            };
            const UInt32 indices[kBoxIndexCount] = {
                0, 1, 2, 2, 3, 0,
                1, 5, 6, 6, 2, 1,
                5, 4, 7, 7, 6, 5,
                4, 0, 3, 3, 7, 4,
                3, 2, 6, 6, 7, 3,
                4, 5, 1, 1, 0, 4,
            };

            DynamicArray<std::byte> vertex_bytes(kBoxVertexByteSize);
            for (Size_t vertex_index = 0; vertex_index < kBoxVertexCount; ++vertex_index) {
                const Size_t base_offset = vertex_index * kBoxVertexStride;
                std::memcpy(vertex_bytes.data() + base_offset, &positions[vertex_index], sizeof(Vector3f));
                const UInt32 normal = 0;
                std::memcpy(vertex_bytes.data() + base_offset + sizeof(Vector3f), &normal, sizeof(UInt32));
                const Vector2f uv(0.0f);
                std::memcpy(vertex_bytes.data() + base_offset + sizeof(Vector3f) + sizeof(UInt32), &uv, sizeof(Vector2f));
            }

            MeshLODData lod{};
            auto vertex_buffer_desc = GfxBufferDesc()
                .setByteSize(kBoxVertexByteSize)
                .setIsVertexBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                .setDebugName("Physics Debug Vertex Buffer");
            lod.buffers.vertex_buffer = GDrawCommandList.createBuffer(vertex_buffer_desc, vertex_bytes.data(), kBoxVertexByteSize);

            auto index_buffer_desc = GfxBufferDesc()
                .setByteSize(kBoxIndexByteSize)
                .setIsIndexBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::IndexBuffer)
                .setDebugName("Physics Debug Index Buffer");
            lod.buffers.index_buffer = GDrawCommandList.createBuffer(index_buffer_desc, indices, kBoxIndexByteSize);

            SubMesh section{};
            section.index_offset = 0;
            section.vertex_offset = 0;
            section.index_count = static_cast<UInt32>(kBoxIndexCount);
            section.vertex_count = static_cast<UInt32>(kBoxVertexCount);
            section.section_index = 0;
            section.primitive_type = MeshGeometryPrimitiveType::Triangles;
            lod.sub_meshes.push_back(section);

            auto mesh = create_scope<Mesh>();
            mesh->setName("physics_debug_box");
            DynamicArray<MeshLODData> lods;
            lods.push_back(std::move(lod));
            mesh->setLODData(lods);
            mesh->setBounds(Vector3f(-0.5f, -0.5f, -0.5f), Vector3f(0.5f, 0.5f, 0.5f));
            return mesh;
        }

        const Mesh* GetUnitBoxMesh() {
            static Scope<Mesh> s_unit_box = BuildUnitBoxMesh();
            return s_unit_box.get();
        }

        PPtr<Material> GetColorMaterial(const UInt32 rgba) {
            static UnorderedMap<UInt32, Scope<Material>> s_materials;
            Scope<Material>& material = s_materials[rgba];
            if (!material) {
                material = create_scope<Material>(ObjectID{UUID::Generate(), 0});
                material->setColor(ColorToVector4(rgba));
            }
            return PPtr<Material>(material.get());
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
        object->setMesh(GetUnitBoxMesh(), 0);

        DynamicArray<PPtr<Material>> materials;
        materials.push_back(GetColorMaterial(line.color));
        object->setOverrideMaterials(materials);
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
