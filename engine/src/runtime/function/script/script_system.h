#pragma once

#include "dopch.h"

#include "script_engine.h"
#include "script_runtime.h"
#include "tooling/tool_interpreter.h"

namespace dodoe {

    struct ScriptSystemCreateInfo {

    };

    class ScriptSystem : public Managed<ScriptSystem, ScriptSystemCreateInfo> {
        friend class Managed<ScriptSystem, ScriptSystemCreateInfo>;
        Scope<ScriptEngine> m_script_engine;
        Scope<ScriptRuntime> m_script_runtime;
        Scope<ToolInterpreter> m_tool_interp;

    public:
        [[nodiscard]] ScriptEngine* getScriptEngine() const { return m_script_engine.get(); }
        [[nodiscard]] ScriptRuntime* getScriptRuntime() const { return m_script_runtime.get(); }

        Bool reloadScripts();

        bool DODOE_API listToolActions(DynamicArray<String>& out_actions);
        bool DODOE_API invokeToolAction(const String& action_name, String& out_error);

        [[nodiscard]] Bool Execute(const FsPath& path);

    private:
        Bool initialize(const ScriptSystemCreateInfo& create_info);
        void shutdown();
    };

} // dodoe
