// do@Redlive

#pragma once

#include "dopch.h"

#include "script_engine.h"
#include "script_runtime.h"

namespace dodoe {

    struct ScriptSystemCreateInfo {

    };

    class ScriptSystem : public Managed<ScriptSystem, ScriptSystemCreateInfo> {
        friend class Managed<ScriptSystem, ScriptSystemCreateInfo>;
        Scope<ScriptEngine> m_script_engine;
        Scope<ScriptRuntime> m_script_runtime;

        struct ToolInterpreter;
        ToolInterpreter* m_tool_interp = nullptr;

    public:
        ScriptSystem();
        ~ScriptSystem();

        [[nodiscard]] ScriptEngine* getMonoEngine() const { return m_script_engine.get(); }
        [[nodiscard]] ScriptRuntime* getMonoRuntime() const { return m_script_runtime.get(); }

        Bool reloadScripts();

        // Execute script by extension: .py
        [[nodiscard]] Bool Execute(const std::filesystem::path& path);

    private:
        Bool initialize(const ScriptSystemCreateInfo& create_info);
        void shutdown();
    };

} // dodoe
