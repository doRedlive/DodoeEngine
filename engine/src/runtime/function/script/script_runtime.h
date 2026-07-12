// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    class ScriptEngine;
    using ScriptCallFn = int (*)(const char* method, void** args, void** result);

    struct ComponentTypeInfo {
        String full_name;
        String ns;
        String name;
    };

    struct ScriptRuntimeCreateInfo {
        ScriptEngine* script_engine;
    };

    class ScriptRuntime : public Managed<ScriptRuntime, ScriptRuntimeCreateInfo> {
        friend class Managed<ScriptRuntime, ScriptRuntimeCreateInfo>;
        ScriptEngine* m_script_engine{nullptr};
        ScriptCallFn m_call{nullptr};

        UnorderedMap<String, ComponentTypeInfo> m_system_class_umap;
        UnorderedMap<String, ComponentTypeInfo> m_component_class_umap;
        UnorderedMap<String, i64> m_system_instance_handles;
        UnorderedMap<ui64, DynamicArray<Pair<String, String>>> m_field_snapshot;

    public:
        Int logSystemClassCount() { return m_system_class_umap.size(); }

        void loadAssemblyClasses();
        void createSystemInstances();
        void reloadAssemblyClasses();
        void clearRuntimeState();
        void snapshotFields();
        void restoreFields();

        void loadEntityMonoComponentsFromManaged(uint64_t entity_uuid);
        bool addEntityMonoComponentFromManaged(uint64_t entity_uuid, const std::string& full_name);
        void removeEntityFromManagedWorld(uint64_t entity_uuid);
        [[nodiscard]] const std::unordered_map<std::string, ComponentTypeInfo>& getComponentClassUmap() const { return m_component_class_umap; }
        [[nodiscard]] const std::unordered_map<ui64, std::vector<std::pair<std::string, std::string>>>& getFieldSnapshot() const { return m_field_snapshot; }

        void onRuntimeStart();
        void onRuntimeUpdate();
        void onRuntimeFinalize();

    private:
        bool initialize(const ScriptRuntimeCreateInfo& info);
        void shutdown();
    };

} // dodoe
