#include "script_runtime.h"

#include "script_engine.h"
#include "runtime/core/utils/json.h"

namespace dodoe {

    namespace {
        using json = Json;
    }

    bool ScriptRuntime::initialize(const ScriptRuntimeCreateInfo &info) {
        m_script_engine = info.script_engine;
        m_call = m_script_engine->getCallFn();

        if (!m_call) {
            DO_ERROR("ScriptRuntime: ScriptHub_Call not available");
            return false;
        }

        m_call("reset_state", nullptr, nullptr);

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
        m_call("scan_types", args, &result);
        if (!result) return;

        String json_str((char*)result);

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

        DO_DEBUG("load assembly classes: system class count {}", m_system_class_umap.size());
    }

    void ScriptRuntime::createSystemInstances() {
        if (!m_call) return;

        for (const auto& [full_name, type_info] : m_system_class_umap) {
            void* args[2] = {
                (void*)type_info.ns.c_str(),
                (void*)type_info.name.c_str()
            };
            void* result = nullptr;
            int rc = m_call("create_instance", args, &result);
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
            m_call("reset_state", nullptr, nullptr);
        }
    }

    void ScriptRuntime::snapshotFields() {
        m_field_snapshot.clear();
        if (!m_call) return;

        void* result = nullptr;
        m_call("snapshot", nullptr, &result);
        if (!result) return;

        String json_str((char*)result);
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
        m_call("restore", args, nullptr);
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

    void ScriptRuntime::loadEntityManagedComponentsFromManaged(uint64_t entity_uuid) {
        if (!m_call) return;

        void* args[1] = { &entity_uuid };
        void* result = nullptr;
        m_call("get_entity_components", args, &result);
    }

    bool ScriptRuntime::getEntityManagedComponentFields(
        uint64_t entity_uuid, DynamicArray<Pair<String, Json>>& out_components) {
        out_components.clear();
        if (!m_call) return false;

        void* args[1] = { &entity_uuid };
        void* result = nullptr;
        const int rc = m_call("get_entity_component_data", args, &result);
        if (rc <= 0 || !result) return false;

        try {
            const Json data = Json::parse(static_cast<const char*>(result));
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
        return m_call("set_entity_component_data", args, nullptr) == 1;
    }

    bool ScriptRuntime::addEntityManagedComponentFromManaged(uint64_t entity_uuid, const String& full_name) {
        if (!m_call) return false;

        void* args[2] = { &entity_uuid, (void*)full_name.c_str() };
        void* result = nullptr;
        int rc = m_call("add_entity_component", args, &result);
        return rc == 1;
    }

    bool ScriptRuntime::removeEntityManagedComponentFromManaged(uint64_t entity_uuid, const String& full_name) {
        if (!m_call) return false;

        void* args[2] = { &entity_uuid, (void*)full_name.c_str() };
        void* result = nullptr;
        int rc = m_call("remove_entity_component", args, &result);
        return rc == 1;
    }

    void ScriptRuntime::removeEntityFromManagedWorld(uint64_t entity_uuid) {
        if (!m_call) return;

        void* args[1] = { &entity_uuid };
        m_call("remove_entity", args, nullptr);
    }

    void ScriptRuntime::onRuntimeStart() {
        if (m_call) {
            m_call("invoke_start", nullptr, nullptr);
        }
    }

    void ScriptRuntime::onRuntimeUpdate() {
        if (m_call) {
            m_call("invoke_update", nullptr, nullptr);
        }
    }

    void ScriptRuntime::onRuntimeFinalize() {
        if (m_call) {
            m_call("invoke_finalize", nullptr, nullptr);
        }
    }

} // dodoe
