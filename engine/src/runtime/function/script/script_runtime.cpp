// do@GreenMuffin

#include "script_runtime.h"

#include "script_class.h"
#include "mono/metadata/class.h"
#include "mono/metadata/object.h"

namespace dodoe {

    Scope<ScriptRuntime> ScriptRuntime::create(const ScriptRuntimeCreateInfo &info) {
        if (auto context = create_scope<ScriptRuntime>(); context->initialize(info))
            return context;
        return nullptr;
    }

    void ScriptRuntime::destroy(Scope<ScriptRuntime> &runtime) {
        if (!runtime) { return; }
        runtime->shutdown();
        runtime.reset();
    }

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

        return true;
    }

    void ScriptRuntime::shutdown() {

    }

    void ScriptRuntime::loadAssemblyClasses() {
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
            Ref<ScriptInstance> script_instance = create_ref<ScriptInstance>(script_class);
            m_system_instance_umap[full_name] = script_instance;
        }
    }

    void ScriptRuntime::onRuntimeStart() {
        for (auto& [_, system] : m_system_instance_umap) {
            system->invokeStart();  
        }
    }

    void ScriptRuntime::onRuntimeUpdate() {
        for (auto& [_, system] : m_system_instance_umap) {
            system->invokeUpdate();  
        }
    }

    void ScriptRuntime::onRuntimeFinalize() {
        for (auto& [_, system] : m_system_instance_umap) {
            system->invokeFinalize();  
        }
    }
    
} // dodoe
