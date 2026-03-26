//
// Created by Redlive on 2026/3/26.
//

#ifndef DODOE_PHSICS_DEBUG_H
#define DODOE_PHSICS_DEBUG_H

#include "dopch.h"

#include "box2d/box2d.h"

namespace dodoe {

    struct PhysicsDebuggerCreateInfo {
        float line_thickness{1.0f};
        float point_size{4.0f};
        float axis_length{0.5f};
    };

    struct DebugDrawContext {
        float line_thickness{2.0f};
        float point_size{4.0f};
        float axis_length{0.5f};
    };

    class PhysicsDebugger {
    public:
        static Scope<PhysicsDebugger> create(const PhysicsDebuggerCreateInfo& create_info);
        static void destroy(Scope<PhysicsDebugger>& debugger);

        [[nodiscard]] b2DebugDraw* native_debug_draw() { return &debug_draw_; }

    private:
        b2DebugDraw debug_draw_{};
        DebugDrawContext debug_draw_context_{};

        void initialize(const PhysicsDebuggerCreateInfo& create_info);
        void shutdown();

    };

} // dodoe

#endif//DODOE_PHSICS_DEBUG_H
