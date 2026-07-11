// do@GreenMuffin

#include "script_runtime.h"

#include "script_class.h"
#include "mono/jit/jit.h"
#include "mono/metadata/appdomain.h"
#include "mono/metadata/class.h"
#include "mono/metadata/debug-helpers.h"
#include "mono/metadata/reflection.h"
#include "mono/metadata/object.h"
#include "mono/utils/mono-publib.h"

namespace dodoe {

    bool ScriptRuntime::initialize(const ScriptRuntimeCreateInfo &info) {
        m_script_engine = info.script_engine;

        MonoClass* world_class = mono_class_from_name(m_script_engine->getCoreImage(), "GreenCake", "World");
        m_class_system = mono_class_from_name(m_script_engine->getCoreImage(), "GreenCake", "DoSystem");

        if (!world_class || !m_class_system) {
            return false;
        }

        MonoObject* world_instance = mono_object_new(m_script_engine->getCoreDomain(), world_class);
        if (!world_instance) {
            return false;
        }
        mono_runtime_object_init(world_instance);

        MonoClass* mb_sys_class = mono_class_from_name(m_script_engine->getCoreImage(), "GreenCake", "MonoBehaviourSystem");
        if (mb_sys_class && mono_class_is_subclass_of(mb_sys_class, m_class_system, false)) {
            auto script_class = create_ref<ScriptClass>(m_script_engine, "GreenCake", "MonoBehaviourSystem", true);
            auto instance = create_ref<MonoSystemInstance>(script_class);
            m_system_instance_umap["GreenCake.MonoBehaviourSystem"] = std::move(instance);
        }

        return true;
    }

    void ScriptRuntime::shutdown() {

    }

    void ScriptRuntime::loadMonoComponentClasses() {
        if (!m_script_engine) {
            return;
        }

        m_component_class_umap.clear();

        MonoClass* component_base = mono_class_from_name(m_script_engine->getCoreImage(), "GreenCake", "Component");
        if (!component_base) {
            return;
        }

        const MonoTableInfo* type_def_tables = mono_image_get_table_info(m_script_engine->getAppImage(), MONO_TABLE_TYPEDEF);
        i32 num_types = mono_table_info_get_rows(type_def_tables);
        for (i32 i = 0; i < num_types; i++) {
            ui32 col[MONO_TYPEDEF_SIZE];
            mono_metadata_decode_row(type_def_tables, i, col, MONO_TYPEDEF_SIZE);

            const char* space_name = mono_metadata_string_heap(m_script_engine->getAppImage(), col[MONO_TYPEDEF_NAMESPACE]);
            const char* class_name = mono_metadata_string_heap(m_script_engine->getAppImage(), col[MONO_TYPEDEF_NAME]);
            MonoClass* mono_class = mono_class_from_name(m_script_engine->getAppImage(), space_name, class_name);
            if (!mono_class) {
                continue;
            }

            if (mono_class == component_base) {
                continue;
            }
            if (!mono_class_is_subclass_of(mono_class, component_base, false)) {
                continue;
            }

            std::string full_name;
            if (space_name && strlen(space_name) > 0)
                full_name = fmt::format("{}.{}", space_name, class_name);
            else
                full_name = class_name ? class_name : "";

            if (full_name.empty()) {
                continue;
            }

            m_component_class_umap[full_name] = create_ref<ScriptClass>(m_script_engine, space_name ? space_name : "", class_name ? class_name : "");
        }
    }

    void ScriptRuntime::loadAssemblyClasses() {
        m_system_class_umap.clear();
        m_system_instance_umap.clear();

        if (!m_script_engine || !m_script_engine->getAppImage()) {
            return;
        }

        loadMonoComponentClasses();

        const MonoTableInfo* type_def_tables = mono_image_get_table_info(m_script_engine->getAppImage(), MONO_TABLE_TYPEDEF);
        i32 num_types = mono_table_info_get_rows(type_def_tables);

        for (i32 i = 0; i < num_types; i++) {
            ui32 col[MONO_TYPEDEF_SIZE];
            mono_metadata_decode_row(type_def_tables, i, col, MONO_TYPEDEF_SIZE);

            const char* space_name = mono_metadata_string_heap(m_script_engine->getAppImage(), col[MONO_TYPEDEF_NAMESPACE]);
            const char* class_name = mono_metadata_string_heap(m_script_engine->getAppImage(), col[MONO_TYPEDEF_NAME]);
            std::string full_name;
            if (strlen(space_name) != 0)
                full_name = fmt::format("{}.{}", space_name, class_name);
            else
                full_name = class_name;

            MonoClass* mono_class = mono_class_from_name(m_script_engine->getAppImage(), space_name, class_name);

            if (mono_class == m_class_system)
                continue;
            if (const bool is_system = mono_class_is_subclass_of(mono_class, m_class_system, false); !is_system)
                continue;

            Ref<ScriptClass> script_class = create_ref<ScriptClass>(m_script_engine, space_name, class_name);
            m_system_class_umap[full_name] = script_class;
            Ref<MonoSystemInstance> script_instance = create_ref<MonoSystemInstance>(script_class);
            m_system_instance_umap[full_name] = script_instance;
        }
        DO_DEBUG("load assembly classes:: system class count{}", static_cast<Int>(m_system_class_umap.size()));
        DO_DEBUG("load assembly classes:: system instance count{}", static_cast<Int>(m_system_instance_umap.size()));
    }

    void ScriptRuntime::clearRuntimeState() {
        m_component_instance_umap.clear();
        m_component_class_umap.clear();
        m_system_class_umap.clear();
        m_system_instance_umap.clear();
        m_class_system = nullptr;
    }

    void ScriptRuntime::snapshotFields() {
        m_field_snapshot.clear();

        for (const auto& [entity_uuid, instances] : m_component_instance_umap) {
            auto& snapshots = m_field_snapshot[entity_uuid];
            for (const auto& instance : instances) {
                if (!instance || !instance->getScriptClass()) continue;
                const auto json = instance->serializeFields();
                snapshots.emplace_back(instance->getScriptClass()->getFullName(), json.dump());
            }
        }
    }

    void ScriptRuntime::restoreFields() {
        for (const auto& [entity_uuid, snapshots] : m_field_snapshot) {
            for (const auto& [type_name, json_str] : snapshots) {
                addEntityMonoComponentFromManaged(static_cast<uint64_t>(entity_uuid), type_name);
            }

            loadEntityMonoComponentsFromManaged(static_cast<uint64_t>(entity_uuid));

            auto it = m_component_instance_umap.find(entity_uuid);
            if (it == m_component_instance_umap.end()) continue;

            for (auto& instance : it->second) {
                if (!instance || !instance->getScriptClass()) continue;
                const auto& full_name = instance->getScriptClass()->getFullName();
                for (const auto& [snap_type, json_str] : snapshots) {
                    if (snap_type == full_name) {
                        instance->deserializeFields(Json::parse(json_str));
                        break;
                    }
                }
            }
        }
    }

    void ScriptRuntime::reloadAssemblyClasses() {
        m_component_instance_umap.clear();
        m_component_class_umap.clear();
        m_system_class_umap.clear();
        m_system_instance_umap.clear();

        if (!m_script_engine || !m_script_engine->getCoreImage()) {
            m_class_system = nullptr;
            DO_ERROR("script engine is null || script engine core image is null");
            return;
        }

        m_class_system = mono_class_from_name(m_script_engine->getCoreImage(), "GreenCake", "DoSystem");
        if (!m_class_system || !m_script_engine->getAppImage()) {
            DO_ERROR("dosystem is null || APP image is null");
            return;
        }

        loadAssemblyClasses();
    }

    void ScriptRuntime::loadEntityMonoComponentsFromManaged(uint64_t entity_uuid) {
        auto& entity_components = m_component_instance_umap[static_cast<ui64>(entity_uuid)];
        entity_components.clear();

        if (!m_script_engine) {
            return;
        }

        MonoClass* external_calls_class = mono_class_from_name(m_script_engine->getCoreImage(), "GreenCake", "ExternalCalls");
        if (!external_calls_class) {
            DO_ERROR("Could not find GreenCake.ExternalCalls in core image.");
            return;
        }

        MonoMethod* get_mono_components = mono_class_get_method_from_name(external_calls_class, "GetEntityMonoComponents", 1);
        if (!get_mono_components) {
            DO_ERROR("Missing GreenCake.ExternalCalls.GetEntityMonoComponents(ulong).");
            return;
        }

        void* call_args[1] = { &entity_uuid };
        MonoObject* exception = nullptr;
        MonoObject* raw = mono_runtime_invoke(get_mono_components, nullptr, call_args, &exception);
        if (exception) {
            MonoString* ex_str = mono_object_to_string(exception, nullptr);
            char* utf8 = ex_str ? mono_string_to_utf8(ex_str) : nullptr;
            DO_ERROR("Managed exception in ExternalCalls.GetEntityMonoComponents: {}", utf8 ? utf8 : "<unknown>");
            if (utf8) mono_free(utf8);
            return;
        }

        MonoArray* component_array = reinterpret_cast<MonoArray*>(raw);
        if (!component_array) {
            return;
        }

        const uintptr_t length = mono_array_length(component_array);
        for (uintptr_t i = 0; i < length; ++i) {
            MonoObject* component_object = mono_array_get(component_array, MonoObject*, i);
            if (!component_object) {
                continue;
            }

            MonoClass* mono_component_class = mono_object_get_class(component_object);
            if (!mono_component_class) {
                continue;
            }

            const char* ns = mono_class_get_namespace(mono_component_class);
            const char* name = mono_class_get_name(mono_component_class);
            std::string full_name = (ns && strlen(ns) > 0) ? fmt::format("{}.{}", ns, name) : std::string(name ? name : "");
            if (full_name.empty()) {
                continue;
            }

            Ref<ScriptClass> script_class = create_ref<ScriptClass>(m_script_engine, ns ? ns : "", name ? name : "");
            Ref<MonoComponentInstance> instance = create_ref<MonoComponentInstance>(script_class, component_object);
            entity_components.emplace_back(std::move(instance));
        }
    }

    bool ScriptRuntime::addEntityMonoComponentFromManaged(uint64_t entity_uuid, const std::string& full_name) {
        if (!m_script_engine) {
            return false;
        }

        MonoClass* external_calls_class = mono_class_from_name(m_script_engine->getCoreImage(), "GreenCake", "ExternalCalls");
        if (!external_calls_class) {
            DO_ERROR("Could not find GreenCake.ExternalCalls in core image.");
            return false;
        }

        MonoMethod* add_component = mono_class_get_method_from_name(external_calls_class, "AddEntityMonoComponent", 2);
        if (!add_component) {
            DO_ERROR("Missing GreenCake.ExternalCalls.AddEntityMonoComponent(ulong,string).");
            return false;
        }

        MonoString* component_name = mono_string_new(m_script_engine->getCoreDomain(), full_name.c_str());
        void* args[2] = { &entity_uuid, component_name };

        MonoObject* exception = nullptr;
        MonoObject* result = mono_runtime_invoke(add_component, nullptr, args, &exception);
        if (exception) {
            MonoString* ex_str = mono_object_to_string(exception, nullptr);
            char* utf8 = ex_str ? mono_string_to_utf8(ex_str) : nullptr;
            DO_ERROR("Managed exception in ExternalCalls.AddEntityMonoComponent: {}", utf8 ? utf8 : "<unknown>");
            if (utf8) mono_free(utf8);
            return false;
        }

        if (!result) {
            return false;
        }
        return *static_cast<bool*>(mono_object_unbox(result));
    }

    void ScriptRuntime::removeEntityFromManagedWorld(uint64_t entity_uuid) {
        m_component_instance_umap.erase(static_cast<ui64>(entity_uuid));

        if (!m_script_engine) {
            return;
        }

        MonoClass* external_calls_class = mono_class_from_name(m_script_engine->getCoreImage(), "GreenCake", "ExternalCalls");
        if (!external_calls_class) {
            DO_ERROR("Could not find GreenCake.ExternalCalls in core image.");
            return;
        }

        MonoMethod* remove_entity = mono_class_get_method_from_name(external_calls_class, "RemoveEntityFromManagedWorld", 1);
        if (!remove_entity) {
            DO_ERROR("Missing GreenCake.ExternalCalls.RemoveEntityFromManagedWorld(ulong).");
            return;
        }

        void* args[1] = { &entity_uuid };
        MonoObject* exception = nullptr;
        mono_runtime_invoke(remove_entity, nullptr, args, &exception);
        if (exception) {
            MonoString* ex_str = mono_object_to_string(exception, nullptr);
            char* utf8 = ex_str ? mono_string_to_utf8(ex_str) : nullptr;
            DO_ERROR("Managed exception in ExternalCalls.RemoveEntityFromManagedWorld: {}", utf8 ? utf8 : "<unknown>");
            if (utf8) mono_free(utf8);
        }
    }

    void ScriptRuntime::onRuntimeStart() {
        if (m_script_engine && m_script_engine->getCoreDomain()) {
            mono_domain_set(m_script_engine->getCoreDomain(), true);
        }
        for (auto& [_, system] : m_system_instance_umap) {
            system->invokeStart();
        }
    }

    void ScriptRuntime::onRuntimeUpdate() {
        if (m_script_engine && m_script_engine->getCoreDomain()) {
            mono_domain_set(m_script_engine->getCoreDomain(), true);
        }
        for (auto& [_, system] : m_system_instance_umap) {
            system->invokeUpdate();
        }
    }

    void ScriptRuntime::onRuntimeFinalize() {
        if (m_script_engine && m_script_engine->getCoreDomain()) {
            mono_domain_set(m_script_engine->getCoreDomain(), true);
        }
        for (auto& [_, system] : m_system_instance_umap) {
            system->invokeFinalize();
        }
    }
    
} // dodoe
