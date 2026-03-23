//
// Created by GreenMuffin on 2026/3/20.
//

#include "script_system.h"

#include "lua/lua_script_runtime.h"

namespace {

    std::string script_language_type2name(const dodoe::ScriptLanguage language) {
        switch (language) {
            case dodoe::ScriptLanguage::Lua: return "Lua";
            case dodoe::ScriptLanguage::CSharp: return "C#";
            default: return "Unknown";
        }
    }

}

namespace dodoe {

    Scope<IScriptRuntime> create_script_runtime(const ScriptLanguage language) {
        switch (language) {
            case ScriptLanguage::Lua:
                return create_scope<LuaScriptRuntime>();
            case ScriptLanguage::CSharp:
                DoWarn("ScriptSystem: script language {} is not available right now.", script_language_type2name(language));
                return nullptr;
            default:
                return nullptr;
        }
    }

    Scope<ScriptSystem> ScriptSystem::create(ScriptSystemCreateInfo create_info) {
        auto context = create_scope<ScriptSystem>();
        context->initialize(std::move(create_info));
        return context;
    }

    void ScriptSystem::destroy(Scope<ScriptSystem>& script_system) {
        if (!script_system) {
            return;
        }
        script_system->shutdown();
        script_system.reset();
    }

    bool ScriptSystem::has_language(const ScriptLanguage language) const {
        return runtimes_.contains(language);
    }

    bool ScriptSystem::execute(const std::filesystem::path& script_file, const ScriptLanguage language) {
        auto* runtime = get_runtime(language);
        if (!runtime) {
            return false;
        }
        return runtime->execute(script_file);
    }

    void ScriptSystem::initialize(ScriptSystemCreateInfo create_info) {
        runtimes_.clear();

        for (const auto language : create_info.languages) {
            if (runtimes_.contains(language)) {
                continue;
            }

            auto script_runtime = create_script_runtime(language);
            if (!script_runtime) {
                continue;
            }

            if (!script_runtime->initialize()) {
                DoError("ScriptSystem: script language {} initialize failed.", script_language_type2name(language));
                continue;
            }

            runtimes_.emplace(language, std::move(script_runtime));
        }

        if (runtimes_.empty()) {
            DoWarn("ScriptSystem: no script language is available.");
        }
    }

    void ScriptSystem::shutdown() {
        for (auto& [_, script_runtime] : runtimes_) {
            script_runtime->shutdown();
        }
        runtimes_.clear();
    }

    IScriptRuntime* ScriptSystem::get_runtime(const ScriptLanguage language) {
        const auto it = runtimes_.find(language);
        if (it == runtimes_.end()) {
            DoError("ScriptSystem: script language {} is not available.", script_language_type2name(language));
            return nullptr;
        }
        return it->second.get();
    }

    const IScriptRuntime* ScriptSystem::get_runtime(const ScriptLanguage language) const {
        const auto it = runtimes_.find(language);
        if (it == runtimes_.end()) {
            DoError("ScriptSystem: script language {} is not available.", script_language_type2name(language));
            return nullptr;
        }
        return it->second.get();
    }

} // dodoe
