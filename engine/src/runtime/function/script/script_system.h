// 
// Created by GreenMuffin on 2026/3/20.
//

#ifndef DODOE_SCRIPT_SYSTEM_H
#define DODOE_SCRIPT_SYSTEM_H

#include "dopch.h"
#include "script_runtime.h"

namespace dodoe {

    struct ScriptSystemCreateInfo {
        std::vector<ScriptLanguage> languages{ ScriptLanguage::Lua };
    };

    class ScriptSystem {
    public:
        static Scope<ScriptSystem> create(ScriptSystemCreateInfo create_info);
        static void destroy(Scope<ScriptSystem>& script_system);

        bool has_language(ScriptLanguage language) const;
        bool execute(const std::filesystem::path& script_file, ScriptLanguage language = ScriptLanguage::Lua);

    private:
        IScriptRuntime* get_runtime(ScriptLanguage language);
        const IScriptRuntime* get_runtime(ScriptLanguage language) const;

        void initialize(ScriptSystemCreateInfo create_info);
        void shutdown();

        std::unordered_map<ScriptLanguage, Scope<IScriptRuntime>, ScriptLanguageHash> runtimes_{};
    };

} // dodoe

#endif//DODOE_SCRIPT_SYSTEM_H
