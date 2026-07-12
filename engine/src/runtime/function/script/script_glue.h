// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    class ScriptEngine;

    class ScriptGlue {
    public:
        static void Initialize(ScriptEngine* engine);
        static void Shutdown();

        static void Register();
        
    private:
        static void RegisterComponents();
        static void RegisterNativeBindings();
    };

} // dodoe
