// do@GreenMuffin

#pragma once

#include "dopch.h"

extern "C" {
    typedef struct _MonoClass MonoClass;
    typedef struct _MonoObject MonoObject;
    typedef struct _MonoType MonoType;
}

namespace dodoe {

    class Entity;
    class ScriptEngine;

    class ScriptGlue {
    public:
        static void Initialize(ScriptEngine* engine);
        static void Shutdown();

        static void Register();

    private:
        static void RegisterComponents();
        static void RegisterFunctions();
    };

} // dodoe