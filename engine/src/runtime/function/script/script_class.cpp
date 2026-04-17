// do@GreenMuffin

#include "script_class.h"

#include "script_engine.h"

namespace dodoe {

    ScriptClass::ScriptClass(ScriptEngine* engine, const std::string& class_namespace, const std::string& class_name, bool is_core) 
        : m_engine(engine), m_class_namespace(class_namespace), m_class_name(class_name) {
        m_mono_class = mono_class_from_name(is_core ? m_engine->getCoreImage() : m_engine->getAppImage(), 
            m_class_namespace.c_str(), m_class_name.c_str());
    }

    MonoObject* ScriptClass::instantiate() {
        MonoObject* instance = mono_object_new(m_engine->getCoreDomain(), m_mono_class);
        mono_runtime_object_init(instance);
        return instance;
    }

} // dodoe