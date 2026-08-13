// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/math/math.h"
#include "runtime/core/utils/uuid.h"

namespace dodoe {

    class PrimitiveRenderObject;

    struct PhysicsDebuggerCreateInfo {
        float line_thickness{1.0f};
        float point_size{4.0f};
    };

    class PhysicsDebugger : public Managed<PhysicsDebugger, PhysicsDebuggerCreateInfo> {
        friend class Managed<PhysicsDebugger, PhysicsDebuggerCreateInfo>;
    public:
        void drawLine(const Vector3f& start, const Vector3f& end, UInt32 color);
        void drawPoint(const Vector3f& position, UInt32 color);
        void flush();

    private:
        struct DebugLine {
            Vector3f start{};
            Vector3f end{};
            UInt32 color{0xFFFFFFFF};
        };
        struct DebugPoint {
            Vector3f position{};
            UInt32 color{0xFFFFFFFF};
        };

        float m_line_thickness{2.0f};
        float m_point_size{4.0f};
        DynamicArray<DebugLine> m_lines{};
        DynamicArray<DebugPoint> m_points{};
        DynamicArray<UUID> m_submitted{};

        bool initialize(const PhysicsDebuggerCreateInfo& create_info);
        void shutdown();

        Scope<PrimitiveRenderObject> buildLineObject(const DebugLine& line) const;
        Scope<PrimitiveRenderObject> buildPointObject(const DebugPoint& point) const;
        Matrix4f buildLineMatrix(const DebugLine& line) const;
    };

} // dodoe
