// do@GreenMuffin

#pragma once

#include "dopch.h"
#include "script_class.h"
#include "script_engine.h"

extern "C" {
    typedef struct _MonoClass MonoClass;
}

namespace dodoe {

    struct ScriptRuntimeCreateInfo {
        ScriptEngine* script_engine;
    };

    class ScriptRuntime {
        ScriptEngine* m_script_engine{nullptr};

        MonoClass* m_class_system{nullptr};
        std::unordered_map<std::string, Ref<ScriptClass>> m_system_class_umap;
        std::unordered_map<std::string, Ref<ScriptInstance>> m_system_instance_umap;
    public:
        static Scope<ScriptRuntime> create(const ScriptRuntimeCreateInfo& info);
        static void destroy(Scope<ScriptRuntime>& runtime);

        void loadAssemblyClasses();

        void onRuntimeStart();
        void onRuntimeUpdate();
        void onRuntimeFinalize();

    private:
        bool initialize(const ScriptRuntimeCreateInfo& info);
        void shutdown();
    };

} // dodoe