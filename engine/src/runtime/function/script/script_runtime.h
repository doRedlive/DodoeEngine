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
        std::unordered_map<std::string, Ref<MonoSystemInstance>> m_system_instance_umap;
        std::unordered_map<std::string, Ref<ScriptClass>> m_component_class_umap;
        std::unordered_map<ui64, std::vector<Ref<MonoComponentInstance>>> m_component_instance_umap;
    public:
        static Scope<ScriptRuntime> create(const ScriptRuntimeCreateInfo& info);
        static void destroy(Scope<ScriptRuntime>& runtime);

        void loadMonoComponentClasses();
        void loadAssemblyClasses();

        void loadEntityMonoComponentsFromManaged(uint64_t entity_uuid);
        bool addEntityMonoComponentFromManaged(uint64_t entity_uuid, const std::string& full_name);
        void removeEntityFromManagedWorld(uint64_t entity_uuid);
        [[nodiscard]] const std::unordered_map<std::string, Ref<ScriptClass>>& getComponentClassUmap() const { return m_component_class_umap; }
        [[nodiscard]] const std::unordered_map<ui64, std::vector<Ref<MonoComponentInstance>>>& getComponentInstanceUmap() const { return m_component_instance_umap; }

        void onRuntimeStart();
        void onRuntimeUpdate();
        void onRuntimeFinalize();

    private:
        bool initialize(const ScriptRuntimeCreateInfo& info);
        void shutdown();
    };

} // dodoe
