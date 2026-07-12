// do@Redlive

#include "debugger.h"

#ifdef DODOE_DEBUG_ENABLED
#include "imgui/imgui.h"
#include "runtime/function/ui/imgui/imgui_builder.h"
#endif

namespace dodoe {

    bool Debugger::initialize(const DebuggerCreateInfo& info) {
        (void)info;
        return true;
    }

    void Debugger::shutdown() {
    }

    Bool Debugger::addImGuiRenderFunc(const String& name, ImGuiRenderFunc func) {
        if (!func) return false;
        for (const auto& pair : m_imguiRenderFuncs) {
            if (pair.first == name) return false;
        }
        m_imguiRenderFuncs.push_back(Pair<String, ImGuiRenderFunc>(name, std::move(func)));
        return true;
    }

    Bool Debugger::removeImGuiRenderFunc(const String& name) {
        auto it = std::find_if(m_imguiRenderFuncs.begin(), m_imguiRenderFuncs.end(),
            [&name](const Pair<String, ImGuiRenderFunc>& pair) { return pair.first == name; });
        if (it != m_imguiRenderFuncs.end()) {
            m_imguiRenderFuncs.erase(it);
            return true;
        }
        return false;
    }

    void Debugger::clearImGuiRenderFuncs() {
        m_imguiRenderFuncs.clear();
    }

    void Debugger::onRender() {
#if defined(DODOE_DEBUG_ENABLED) && !defined(DODOE_EDITOR_ENABLED)
        if (!ImGuiBuilder::GetContext()) return;
        ImGuiContext* prev_ctx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(ImGuiBuilder::GetContext());
        onImGuiRender();
        ImGui::SetCurrentContext(prev_ctx);
#endif
    }

    void Debugger::onImGuiRender() {
#if defined(DODOE_DEBUG_ENABLED) && !defined(DODOE_EDITOR_ENABLED)
        for (const auto& pair : m_imguiRenderFuncs) {
            if (pair.second) {
                pair.second();
            }
        }
#endif
    }

} // dodoe
