// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/core/utils/json.h"

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

    class DODOE_API ScriptRuntime : public Managed<ScriptRuntime, ScriptRuntimeCreateInfo> {
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

        void loadEntityManagedComponentsFromManaged(uint64_t entity_uuid);
        bool getEntityManagedComponentFields(uint64_t entity_uuid,
                                              DynamicArray<Pair<String, Json>>& out_components);
        bool setEntityManagedComponentFields(uint64_t entity_uuid, const String& full_name,
                                              const Json& fields);
        bool addEntityManagedComponentFromManaged(uint64_t entity_uuid, const String& full_name);
        bool removeEntityManagedComponentFromManaged(uint64_t entity_uuid, const String& full_name);
        void removeEntityFromManagedWorld(uint64_t entity_uuid);
        [[nodiscard]] const UnorderedMap<String, ComponentTypeInfo>& getComponentClassUmap() const { return m_component_class_umap; }
        [[nodiscard]] const UnorderedMap<ui64, DynamicArray<Pair<String, String>>>& getFieldSnapshot() const { return m_field_snapshot; }

        void onRuntimeStart();
        void onRuntimeUpdate();
        void onRuntimeFixedUpdate();
        void onRuntimeFinalize();

    private:
        bool initialize(const ScriptRuntimeCreateInfo& info);
        void shutdown();
    };

} // dodoe
