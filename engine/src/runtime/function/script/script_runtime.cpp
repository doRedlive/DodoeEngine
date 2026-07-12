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

        auto* alc = m_script_engine->getAlcHandle();
        void* init_args[1] = { alc };
        void* types_result = nullptr;
        m_call("scan_types", init_args, &types_result);

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

        std::string json_str((char*)result);

        try {
            json types = json::parse(json_str);
            for (const auto& t : types) {
                std::string ns = t.value("ns", "");
                std::string name = t.value("name", "");
                std::string base_ns = t.value("base_ns", "");
                std::string base_name = t.value("base_name", "");
                std::string full_name = ns.empty() ? name : ns + "." + name;

                if (base_ns == "GreenCake" && base_name == "Component") {
                    m_component_class_umap[full_name] = {full_name, ns, name};
                }
                if (base_ns == "GreenCake" && base_name == "DoSystem") {
                    m_system_class_umap[full_name] = {full_name, ns, name};
                }
            }
        } catch (const std::exception& e) {
            DO_ERROR("ScriptRuntime: failed to parse scan_types JSON: {}", e.what());
        }

        if (m_system_class_umap.find("GreenCake.BehaviourSystem") == m_system_class_umap.end()) {
            m_system_class_umap["GreenCake.BehaviourSystem"] = {"GreenCake.BehaviourSystem", "GreenCake", "BehaviourSystem"};
        }

        DO_DEBUG("load assembly classes: system class count {}", m_system_class_umap.size());
    }

    void ScriptRuntime::clearRuntimeState() {
        m_component_class_umap.clear();
        m_system_class_umap.clear();
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

        std::string json_str((char*)result);
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
        std::string json_str = "{}";
        if (!m_field_snapshot.empty()) {
            json restoreJson = json::object();
            for (const auto& [entity_uuid, snapshots] : m_field_snapshot) {
                json fields_obj = json::object();
                for (const auto& [key, value] : snapshots) {
                    fields_obj[key] = value;
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

        if (!m_call) {
            DO_ERROR("ScriptRuntime: ScriptHub_Call not available");
            return;
        }

        loadAssemblyClasses();
    }

    void ScriptRuntime::loadEntityMonoComponentsFromManaged(uint64_t entity_uuid) {
        if (!m_call) return;

        void* args[1] = { &entity_uuid };
        void* result = nullptr;
        m_call("get_entity_components", args, &result);
    }

    bool ScriptRuntime::addEntityMonoComponentFromManaged(uint64_t entity_uuid, const std::string& full_name) {
        if (!m_call) return false;

        void* args[2] = { &entity_uuid, (void*)full_name.c_str() };
        void* result = nullptr;
        int rc = m_call("add_entity_component", args, &result);
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
            DO_DEBUG("m_call is not null");
        }
    }

    void ScriptRuntime::onRuntimeFinalize() {
        if (m_call) {
            m_call("invoke_finalize", nullptr, nullptr);
        }
    }

} // dodoe
