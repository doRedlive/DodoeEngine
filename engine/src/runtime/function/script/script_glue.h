// do@Redlive

#pragma once

#include "dopch.h"
#include "script_command.h"

namespace dodoe {

    class ScriptEngine;

    class DODOE_API ScriptGlue {
    public:
        using HostExtensionFn = void (*)(ScriptCallFn call);

        static void Initialize(ScriptEngine* engine);
        static void Shutdown();

        static void Register();

        static void AddHostExtension(void* token, HostExtensionFn fn);
        static void RemoveHostExtension(void* token);

    private:
        static void RegisterComponents();
        static void RegisterNativeBindings();
    };

} // dodoe
