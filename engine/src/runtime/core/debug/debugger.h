// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/core/memory/managed.h"

#ifdef DODOE_DEBUG
#include "imgui/imgui.h"
#endif

namespace dodoe {

    struct DebuggerCreateInfo {};

    class Debugger : public Managed<Debugger, DebuggerCreateInfo> {
        friend class Managed<Debugger, DebuggerCreateInfo>;
    public:
        void onRender();

    private:
        bool initialize(const DebuggerCreateInfo& info);
        void shutdown();
        void onImGuiRendr();

    };

} // dodoe
