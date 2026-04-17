// do@GreenMuffin

#pragma once

#include "dopch.h"

#include "mono/metadata/class.h"
#include "mono/metadata/object.h"

namespace dodoe{

	class ScriptEngine;

	class ScriptClass {
		ScriptEngine* m_engine;

		std::string m_class_namespace;
		std::string m_class_name;
		MonoClass* m_mono_class{nullptr};
	public:
		ScriptClass() = default;
		ScriptClass(ScriptEngine* engine, const std::string& class_namespace, const std::string& class_name, bool is_core = false);

		MonoObject* instantiate();
		MonoMethod* getMethod(const std::string& name, int parameter_count);
		MonoObject* invokeMethod(MonoObject* instance, MonoMethod* method, void** params = nullptr);	
	};

	class ScriptInstance {
		Ref<ScriptClass> m_script_class;

		MonoObject* m_instance{nullptr};
		MonoMethod* m_constructor{nullptr};
		MonoMethod* m_start{nullptr};
		MonoMethod* m_update{nullptr};
		MonoMethod* m_finalize{nullptr};
	public:
		ScriptInstance(Ref<ScriptClass> script_class);

		void invokeStart();
		void invokeUpdate(float dt);
		void invokeFinalize();

	private:
	};

} // dodoe