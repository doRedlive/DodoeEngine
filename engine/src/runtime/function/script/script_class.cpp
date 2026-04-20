// do@GreenMuffin

#include "script_class.h"

#include "script_engine.h"
#include "mono/metadata/debug-helpers.h"
#include "mono/utils/mono-publib.h"

namespace dodoe {

    ScriptClass::ScriptClass(ScriptEngine* engine, const std::string& class_namespace, const std::string& class_name, bool is_core) : 
        m_engine(engine), m_class_namespace(class_namespace), m_class_name(class_name) {
        m_mono_class = mono_class_from_name(is_core ? m_engine->getCoreImage() : m_engine->getAppImage(), 
            m_class_namespace.c_str(), m_class_name.c_str());
    }

    MonoObject* ScriptClass::instantiate() {
        MonoObject* instance = mono_object_new(m_engine->getCoreDomain(), m_mono_class);
        mono_runtime_object_init(instance);
        return instance;
    }

    MonoMethod* ScriptClass::getMethod(const std::string& name, int parameter_count) {
        return mono_class_get_method_from_name(m_mono_class, name.c_str(), parameter_count);
    }

    MonoObject* ScriptClass::invokeMethod(MonoObject* instance, MonoMethod* method, void** params) {
        if (!method) {
            DO_ERROR("Managed method lookup failed for {}.{}.", m_class_namespace, m_class_name);
            return nullptr;
        }

        MonoObject* exception = nullptr;
        MonoObject* result = mono_runtime_invoke(method, instance, params, &exception);
        if (exception) {
            MonoString* exception_string = mono_object_to_string(exception, nullptr);
            char* exception_chars = exception_string ? mono_string_to_utf8(exception_string) : nullptr;
            DO_ERROR("Managed exception in {}.{}: {}",
                m_class_namespace, m_class_name, exception_chars ? exception_chars : "<unknown>");
            if (exception_chars) {
                mono_free(exception_chars);
            }
        }
        return result;
    }

    ScriptInstance::ScriptInstance(Ref<ScriptClass> script_class) : 
        m_script_class(script_class) {
        m_instance = m_script_class->instantiate();
        m_constructor = m_script_class->getMethod(".ctor", 0);
        m_start = m_script_class->getMethod("Start", 0);
        m_update = m_script_class->getMethod("Update", 0);
        m_finalize = m_script_class->getMethod("Finalize", 0);
    } 

    void ScriptInstance::invokeStart() {
        if (m_start)
            m_script_class->invokeMethod(m_instance, m_start);
    }

    void ScriptInstance::invokeUpdate() {
        if (m_update) 
            m_script_class->invokeMethod(m_instance, m_update);
    }

    void ScriptInstance::invokeFinalize() {
        if (m_finalize)
            m_script_class->invokeMethod(m_instance, m_finalize);
    }

} // dodoe
