#include "script_runtime.h"

#include "script_engine.h"
#include "runtime/core/utils/json.h"

#include <combaseapi.h>

namespace dodoe {

    namespace {
        using json = Json;

        class CoTaskMemResult {
        public:
            explicit CoTaskMemResult(void* ptr) : m_ptr(ptr) {}
            ~CoTaskMemResult() { if (m_ptr) CoTaskMemFree(m_ptr); }
            CoTaskMemResult(const CoTaskMemResult&) = delete;
            CoTaskMemResult& operator=(const CoTaskMemResult&) = delete;
            const char* get() const { return static_cast<const char*>(m_ptr); }
        private:
            void* m_ptr{nullptr};
        };
    }

    bool ScriptRuntime::initialize(const ScriptRuntimeCreateInfo &info) {
        m_script_engine = info.script_engine;
        m_call = m_script_engine->getCallFn();
        m_invoke_start = m_script_engine->getInvokeStartFn();
        m_invoke_update = m_script_engine->getInvokeUpdateFn();
        m_invoke_fixed_update = m_script_engine->getInvokeFixedUpdateFn();
        m_invoke_finalize = m_script_engine->getInvokeFinalizeFn();

        if (!m_call) {
            DO_ERROR("ScriptRuntime: ScriptHub_Call not available");
            return false;
        }

        m_call(ScriptCommand::ResetState, nullptr, nullptr);

        loadAssemblyClasses();

        return true;
    }

    void ScriptRuntime::shutdown() {

    }

    void ScriptRuntime::loadAssemblyClasses() {
        m_system_class_umap.clear();
        m_component_class_umap.clear();

        if (!m_call) return;

        auto* alc = m_script_engine->getAlcHandle();
        void* args[1] = { alc };
        void* result = nullptr;
        m_call(ScriptCommand::ScanTypes, args, &result);
        if (!result) return;

        const CoTaskMemResult owned_result(result);
        String json_str(owned_result.get());

        try {
            json types = json::parse(json_str);
            for (const auto& t : types) {
                String ns = String(t.value("ns", "").c_str());
                String name = String(t.value("name", "").c_str());
                String base_ns = String(t.value("baseNs", "").c_str());
                String base_name = String(t.value("baseName", "").c_str());
                String full_name = ns.empty() ? name : ns + "." + name;

                if (base_ns == "GreenCake" && base_name == "CakeComponent") {
                    m_component_class_umap[full_name] = {full_name, ns, name};
                }
                if (base_ns == "GreenCake" && base_name == "CakeSystem") {
                    m_system_class_umap[full_name] = {full_name, ns, name};
                }
            }
        } catch (const std::exception& e) {
            DO_ERROR("ScriptRuntime: failed to parse scan_types JSON: {}", e.what());
        }

        if (m_system_class_umap.find("GreenCake.CakeBehaviourSystem") == m_system_class_umap.end()) {
            m_system_class_umap["GreenCake.CakeBehaviourSystem"] = {"GreenCake.CakeBehaviourSystem", "GreenCake", "CakeBehaviourSystem"};
        }
    }

    void ScriptRuntime::createSystemInstances() {
        if (!m_call) return;

        for (const auto& [full_name, type_info] : m_system_class_umap) {
            void* args[2] = {
                (void*)type_info.ns.c_str(),
                (void*)type_info.name.c_str()
            };
            void* result = nullptr;
            int rc = m_call(ScriptCommand::CreateInstance, args, &result);
            if (rc == 1) {
                m_system_instance_handles[full_name] = reinterpret_cast<i64>(result);
            }
        }

        DO_DEBUG("create system instances: {}", m_system_instance_handles.size());
    }

    void ScriptRuntime::clearRuntimeState() {
        m_component_class_umap.clear();
        m_system_class_umap.clear();
        m_system_instance_handles.clear();
        if (m_call) {
            m_call(ScriptCommand::ResetState, nullptr, nullptr);
        }
    }

    void ScriptRuntime::snapshotFields() {
        m_field_snapshot.clear();
        if (!m_call) return;

        void* result = nullptr;
        m_call(ScriptCommand::Snapshot, nullptr, &result);
        if (!result) return;

        const CoTaskMemResult owned_result(result);
        String json_str(owned_result.get());
        try {
            json snapshot = json::parse(json_str);
            for (auto& [entityStr, fields_obj] : snapshot.items()) {
                ui64 entity_uuid = std::stoull(entityStr);
                auto& snapshots = m_field_snapshot[entity_uuid];
                for (auto& [fieldName, fieldValue] : fields_obj.items()) {
                    snapshots.emplace_back(fieldName, fieldValue.dump());
                }
            }
        } catch (const std::exception& e) {
            DO_ERROR("ScriptRuntime: failed to parse snapshot JSON: {}", e.what());
        }
    }

    void ScriptRuntime::restoreFields() {
        if (!m_call) return;

        void* args[1] = { nullptr };
        String json_str = "{}";
        if (!m_field_snapshot.empty()) {
            json restoreJson = json::object();
            for (const auto& [entity_uuid, snapshots] : m_field_snapshot) {
                json fields_obj = json::object();
                for (const auto& [key, value] : snapshots) {
                    fields_obj[key.c_str()] = value;
                }
                restoreJson[std::to_string(entity_uuid)] = fields_obj;
            }
            json_str = restoreJson.dump();
        }

        args[0] = (void*)json_str.c_str();
        m_call(ScriptCommand::Restore, args, nullptr);
    }

    void ScriptRuntime::reloadAssemblyClasses() {
        m_component_class_umap.clear();
        m_system_class_umap.clear();
        m_system_instance_handles.clear();

        if (!m_call) {
            DO_ERROR("ScriptRuntime: ScriptHub_Call not available");
            return;
        }

        loadAssemblyClasses();
        createSystemInstances();
    }

    bool ScriptRuntime::listToolActions(DynamicArray<String>& out_actions) {
        out_actions.clear();
        if (!m_call) return false;

        void* result = nullptr;
        int rc = m_call(ScriptCommand::ListToolActions, nullptr, &result);
        if (!result) return false;
        const CoTaskMemResult owned_result(result);
        if (rc != 1) return false;
        String json_str(owned_result.get());
        try {
            json actions = json::parse(json_str);
            if (!actions.is_array()) return false;
            for (const auto& action : actions) {
                if (!action.is_string()) continue;
                out_actions.emplace_back(action.get<std::string>().c_str());
            }
            return true;
        } catch (const std::exception& e) {
            DO_ERROR("ScriptRuntime: failed to parse list_tool_actions JSON: {}", e.what());
            return false;
        }
    }

    bool ScriptRuntime::invokeToolAction(const String& action_name, String& out_error) {
        out_error.clear();
        if (!m_call) return false;

        void* args[1] = { (void*)action_name.c_str() };
        void* result = nullptr;
        int rc = m_call(ScriptCommand::InvokeToolAction, args, &result);
        if (!result) return false;
        const CoTaskMemResult owned_result(result);
        if (rc != 1) return false;
        String json_str(owned_result.get());
        try {
            json response = json::parse(json_str);
            if (!response.value("ok", false)) {
                out_error = String(response.value("error", "").c_str());
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            out_error = String(e.what());
            return false;
        }
    }

    bool ScriptRuntime::fetchScriptGcInfo(ScriptGcInfo& out_info) {
        out_info = {};
        if (!m_call) return false;

        void* result = nullptr;
        const int rc = m_call(ScriptCommand::GcInfo, nullptr, &result);
        if (rc != 1 || !result) return false;

        const CoTaskMemResult owned_result(result);
        try {
            const Json info = Json::parse(owned_result.get());
            out_info.heap_allocated_bytes = info.value("heapAllocatedBytes", 0ull);
            out_info.heap_size_bytes = info.value("heapSizeBytes", 0ull);
            out_info.memory_load_bytes = info.value("memoryLoadBytes", 0ull);
            out_info.gen0_collections = info.value("gen0Collections", 0u);
            out_info.gen1_collections = info.value("gen1Collections", 0u);
            out_info.gen2_collections = info.value("gen2Collections", 0u);
            out_info.assembly_count = info.value("assemblyCount", 0u);
            out_info.object_registry_count = info.value("objectRegistryCount", 0ull);
            out_info.instance_type_cache_count = info.value("instanceTypeCacheCount", 0ull);
            out_info.entity_handle_total = info.value("entityHandleTotal", 0ull);
        }
        catch (const Json::exception& e) {
            DO_ERROR("ScriptRuntime: failed to parse gc_info: {}", e.what());
            return false;
        }
        return true;
    }

    void ScriptRuntime::loadEntityManagedComponentsFromManaged(uint64_t entity_uuid) {
        if (!m_call) return;

        void* args[1] = { &entity_uuid };
        void* result = nullptr;
        m_call(ScriptCommand::GetEntityComponents, args, &result);
        const CoTaskMemResult owned_result(result);
    }

    bool ScriptRuntime::getEntityManagedComponentFields(
        uint64_t entity_uuid, DynamicArray<Pair<String, Json>>& out_components) {
        out_components.clear();
        if (!m_call) return false;

        void* args[1] = { &entity_uuid };
        void* result = nullptr;
        const int rc = m_call(ScriptCommand::GetEntityComponentData, args, &result);
        if (!result) return false;
        const CoTaskMemResult owned_result(result);
        if (rc <= 0) return false;
        try {
            const Json data = Json::parse(owned_result.get());
            if (!data.is_object()) return false;
            for (const auto& [type_name, fields] : data.items()) {
                out_components.emplace_back(String(type_name.c_str()), fields);
            }
        }
        catch (const Json::exception& e) {
            DO_ERROR("ScriptRuntime: failed to parse managed component data: {}", e.what());
            out_components.clear();
            return false;
        }
        return true;
    }

    bool ScriptRuntime::setEntityManagedComponentFields(
        uint64_t entity_uuid, const String& full_name, const Json& fields) {
        if (!m_call || !fields.is_object()) return false;

        const String json_str = fields.dump().c_str();
        void* args[3] = { &entity_uuid, (void*)full_name.c_str(), (void*)json_str.c_str() };
        return m_call(ScriptCommand::SetEntityComponentData, args, nullptr) == 1;
    }

    bool ScriptRuntime::addEntityManagedComponentFromManaged(uint64_t entity_uuid, const String& full_name) {
        if (!m_call) return false;

        void* args[2] = { &entity_uuid, (void*)full_name.c_str() };
        void* result = nullptr;
        int rc = m_call(ScriptCommand::AddEntityComponent, args, &result);
        return rc == 1;
    }

    bool ScriptRuntime::removeEntityManagedComponentFromManaged(uint64_t entity_uuid, const String& full_name) {
        if (!m_call) return false;

        void* args[2] = { &entity_uuid, (void*)full_name.c_str() };
        void* result = nullptr;
        int rc = m_call(ScriptCommand::RemoveEntityComponent, args, &result);
        return rc == 1;
    }

    void ScriptRuntime::removeEntityFromManagedWorld(uint64_t entity_uuid) {
        if (!m_call) return;

        void* args[1] = { &entity_uuid };
        m_call(ScriptCommand::RemoveEntity, args, nullptr);
    }

    void ScriptRuntime::onRuntimeStart() {
        DO_PROFILE_SCOPE_CATEGORY("ScriptRuntime::onRuntimeStart", "script");
        if (m_invoke_start) {
            m_invoke_start();
        } else if (m_call) {
            m_call(ScriptCommand::InvokeStart, nullptr, nullptr);
        }
    }

    void ScriptRuntime::onRuntimeUpdate() {
        DO_PROFILE_SCOPE_CATEGORY("ScriptRuntime::onRuntimeUpdate", "script");
        if (m_invoke_update) {
            m_invoke_update();
        } else if (m_call) {
            m_call(ScriptCommand::InvokeUpdate, nullptr, nullptr);
        }
    }

    void ScriptRuntime::onRuntimeFixedUpdate() {
        DO_PROFILE_SCOPE_CATEGORY("ScriptRuntime::onRuntimeFixedUpdate", "script");
        if (m_invoke_fixed_update) {
            m_invoke_fixed_update();
        } else if (m_call) {
            m_call(ScriptCommand::InvokeFixedUpdate, nullptr, nullptr);
        }
    }

    void ScriptRuntime::onRuntimeFinalize() {
        DO_PROFILE_SCOPE_CATEGORY("ScriptRuntime::onRuntimeFinalize", "script");
        if (m_invoke_finalize) {
            m_invoke_finalize();
        } else if (m_call) {
            m_call(ScriptCommand::InvokeFinalize, nullptr, nullptr);
        }
    }

} // dodoe
