//
// Created by GreenMuffin on 2026/3/x.
//

#ifndef DODOE_SCRIPT_RUNTIME_H
#define DODOE_SCRIPT_RUNTIME_H

#include "dopch.h"

namespace dodoe {

    enum class ScriptLanguage {
        Lua,
        CSharp
    };

    class IScriptRuntime {
    public:
        virtual ~IScriptRuntime() = default;

        virtual ScriptLanguage language() const = 0;
        virtual bool initialize() = 0;
        virtual void shutdown() = 0;

        virtual bool execute(const std::filesystem::path& script_file) = 0;
    };

    struct ScriptLanguageHash {
        std::size_t operator()(const ScriptLanguage type) const {
            return std::hash<int>()(static_cast<int>(type));
        }
    };

} // dodoe

#endif//DODOE_SCRIPT_RUNTIME_H
