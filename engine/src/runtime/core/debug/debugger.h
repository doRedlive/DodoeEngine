// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/core/memory/managed.h"

#ifdef DODOE_DEBUG
#include "imgui/imgui.h"
#endif

namespace dodoe {

    struct DebuggerCreateInfo {};

    using ImGuiRenderFunc = std::function<void()>;

    class Debugger : public Managed<Debugger, DebuggerCreateInfo> {
        friend class Managed<Debugger, DebuggerCreateInfo>;

        DynamicArray<Pair<String, ImGuiRenderFunc>> m_imguiRenderFuncs;
    public:
        bool addImGuiRenderFunc(const String& name, ImGuiRenderFunc func);
        bool removeImGuiRenderFunc(const String& name);
        void clearImGuiRenderFuncs();

        void onRender();

    private:
        bool initialize(const DebuggerCreateInfo& info);
        void shutdown();
        void onImGuiRendr();
    };

} // dodoe
