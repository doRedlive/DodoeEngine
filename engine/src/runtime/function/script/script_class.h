// do@GreenMuffin

#pragma once

#include "dopch.h"

#include "mono/metadata/class.h"
#include "mono/metadata/object.h"

namespace dodoe{

	class ScriptEngine;

	enum class ScriptFieldType {
		None = 0,
		Float, Double,
		Bool, Char, Byte, Short, Int, Long,
		UByte, UShort, UInt, ULong,
		Vector2, Vector3, Vector4,
		Entity
	};

	struct ScriptField {
		ScriptFieldType type{};
		std::string name;
		MonoClassField* mono_field{nullptr};
	};

	class ScriptFieldInstance {
		ScriptField m_field;
		uint8_t m_buffer[16]{};
	public:
		ScriptFieldInstance() {
			memset(m_buffer, 0, sizeof(m_buffer));
		}

		template <typename T>
		T getValue() {
			static_assert(sizeof(T) <= 16, "Type too large!");
			return *static_cast<T *>(m_buffer);
		}

		template <typename T>
		void setValue(T value) {
			static_assert(sizeof(T) <= 16, "Type tool large!");
			memcpy(m_buffer, &value, sizeof(T));
		}
	};

	class ScriptClass {
		ScriptEngine* m_engine{nullptr};

		std::string m_class_namespace;
		std::string m_class_name;
		std::map<std::string, ScriptField> m_fields{};

		MonoClass* m_mono_class{nullptr};
	public:
		ScriptClass() = default;
		ScriptClass(ScriptEngine* engine, const std::string& class_namespace, const std::string& class_name, bool is_core = false);

		[[nodiscard]] MonoObject* instantiate() const;
		[[nodiscard]] MonoMethod* getMethod(const std::string& name, int parameter_count) const;
		MonoObject* invokeMethod(MonoObject* instance, MonoMethod* method, void** params = nullptr);
		[[nodiscard]] MonoClass* getMonoClass() const { return m_mono_class; }

		[[nodiscard]] const std::map<std::string, ScriptField>& getFields() const { return m_fields; }
	};

	class MonoComponentInstance {
		Ref<ScriptClass> m_script_class;

		MonoObject* m_instance{nullptr};
		// MonoMethod* m_constructor{nullptr};

		inline static char s_field_value_buffer[16];
	public:
		explicit MonoComponentInstance(const Ref<ScriptClass>& script_class);
		MonoComponentInstance(const Ref<ScriptClass>& script_class, MonoObject* instance);
		[[nodiscard]] const Ref<ScriptClass>& getScriptClass() const { return m_script_class; }

		template <typename T>
		[[nodiscard]] T getFieldValue(const std::string& name) {
			static_assert(sizeof(T) <= 16, "Type too large!");
			if (!getFieldValueInternal(name, s_field_value_buffer)) {
				return T();
			}
			T value{};
			std::memcpy(&value, s_field_value_buffer, sizeof(T));
			return value;
		}

		template <typename T>
		void setFieldValue(const std::string& name, T value) {
			static_assert(sizeof(T) <= 16, "Type too large!");
			if (!setFieldValueInternal(name, &value)) {
				DO_ERROR("Set field value failed!");
			}
		}

	private:
		[[nodiscard]] bool getFieldValueInternal(const std::string& name, void* buffer) const;
		[[nodiscard]] bool setFieldValueInternal(const std::string& name, const void* value) const;
	};

	class MonoSystemInstance {
		Ref<ScriptClass> m_script_class;

		MonoObject* m_instance{nullptr};
		MonoMethod* m_constructor{nullptr};
		MonoMethod* m_start{nullptr};
		MonoMethod* m_update{nullptr};
		MonoMethod* m_finalize{nullptr};
	public:
		explicit MonoSystemInstance(const Ref<ScriptClass> &script_class);

		void invokeStart() const;
		void invokeUpdate() const;
		void invokeFinalize() const;
	};

} // dodoe
