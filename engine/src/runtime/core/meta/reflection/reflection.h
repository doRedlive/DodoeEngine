#ifndef DODOE_REFLECTION_HPP
#define DODOE_REFLECTION_HPP

#include "dopch.h"

#include "runtime/core/meta/json.h"

namespace dodoe {

#if defined(__REFLECTION_PARSER__)
#define META(...) __attribute__((annotate(#__VA_ARGS__)))
#define CLASS(class_name, ...) class __attribute__((annotate(#__VA_ARGS__))) class_name
#define STRUCT(struct_name, ...) struct __attribute__((annotate(#__VA_ARGS__))) struct_name
#else
#define META(...)
#define CLASS(class_name, ...) class class_name
#define STRUCT(struct_name, ...) struct struct_name
#endif

#define REFLECTION_BODY(class_name) \
    friend class dodoe::reflection::TypeFieldReflectionOperator::Type##class_name##Operator; \
    friend class dodoe::Serializer;

#define REFLECTION_TYPE(class_name) \
    namespace dodoe::reflection { \
        namespace TypeFieldReflectionOperator { \
            class Type##class_name##Operator; \
        } \
    }

#define REGISTER_FIELD_TO_MAP(name, value) dodoe::reflection::TypeMetaRegisterInterface::register_to_field_map(name, value)
#define REGISTER_METHOD_TO_MAP(name, value) dodoe::reflection::TypeMetaRegisterInterface::register_to_method_map(name, value)
#define REGISTER_BASE_CLASS_TO_MAP(name, value) dodoe::reflection::TypeMetaRegisterInterface::register_to_class_map(name, value)
#define REGISTER_ARRAY_TO_MAP(name, value) dodoe::reflection::TypeMetaRegisterInterface::register_to_array_map(name, value)
#define UNREGISTER_ALL dodoe::reflection::TypeMetaRegisterInterface::unregister_all()

    class Serializer;

    namespace reflection {
        class TypeMeta;
        class FieldAccessor;
        class MethodAccessor;
        class ArrayAccessor;
        class ReflectionInstance;

        using SetFunc = std::function<void(void*, void*)>;
        using GetFunc = std::function<void*(void*)>;
        using GetNameFunc = std::function<const char*()>;
        using SetArrayFunc = std::function<void(int, void*, void*)>;
        using GetArrayFunc = std::function<void*(int, void*)>;
        using GetSizeFunc = std::function<int(void*)>;
        using GetBoolFunc = std::function<bool()>;
        using InvokeFunc = std::function<void(void*)>;

        using ConstructorWithJson = std::function<void*(const Json&)>;
        using WriteJsonByName = std::function<Json(void*)>;
        using GetBaseClassReflectionInstanceListFunc = std::function<int(ReflectionInstance*&, void*)>;

        using FieldFuncTuple = std::tuple<SetFunc, GetFunc, GetNameFunc, GetNameFunc, GetNameFunc, GetBoolFunc>;
        using ClassFuncTuple = std::tuple<GetBaseClassReflectionInstanceListFunc, ConstructorWithJson, WriteJsonByName>;
        using ArrayFuncTuple = std::tuple<SetArrayFunc, GetArrayFunc, GetSizeFunc, GetNameFunc, GetNameFunc>;
        using MethodFuncTuple = std::tuple<GetNameFunc, InvokeFunc>;

        class TypeMetaRegisterInterface {
        public:
            static void register_to_class_map(const char* name, ClassFuncTuple* value);
            static void register_to_field_map(const char* name, FieldFuncTuple* value);
            static void register_to_array_map(const char* name, ArrayFuncTuple* value);
            static void register_to_method_map(const char* name, MethodFuncTuple* value);
            static void unregister_all();

            static void register2classmap(const char* name, ClassFuncTuple* value);
            static void register2fieldmap(const char* name, FieldFuncTuple* value);
            static void register2arraymap(const char* name, ArrayFuncTuple* value);
            static void register2methodmap(const char* name, MethodFuncTuple* value);
        };

        class TypeMeta {
            friend class FieldAccessor;
            friend class ArrayAccessor;
            friend class TypeMetaRegisterInterface;
        public:
            TypeMeta();

            static TypeMeta new_meta_from_name(const std::string& name);
            static bool new_array_accessor_from_name(const std::string& array_type_name, ArrayAccessor& accessor);
            static ReflectionInstance new_from_name_and_json(const std::string& type_name, const Json& json_context);
            static Json write_by_name(const std::string& name, void* instance);

            static ReflectionInstance newFromNameAndJson(const std::string& type_name, const Json& json_context);
            static Json writeByName(const std::string& name, void* instance);

            const std::string& get_type_name();
            int get_field_list(FieldAccessor*& out_list);
            int get_method_list(MethodAccessor*& out_list);
            int get_base_class_reflection_instance_list(ReflectionInstance*& out_list, void* instance);
            FieldAccessor get_field_by_name(const char* name);
            MethodAccessor get_method_by_name(const char* name);
            bool is_valid() const { return is_valid_; }
            TypeMeta& operator=(const TypeMeta& dest);

        private:
            std::vector<FieldAccessor> fields_;
            std::vector<MethodAccessor> methods_;
            std::string type_name_;
            bool is_valid_{false};

            explicit TypeMeta(const std::string& type_name);
        };

        class FieldAccessor {
            friend class TypeMeta;
        public:
            FieldAccessor();

            void* get(void* instance);
            void set(void* instance, void* value);
            TypeMeta get_owner_type_meta();
            bool get_type_meta(TypeMeta& field_type);
            const char* get_field_name() const;
            const char* get_field_type_name();
            bool is_array_type();
            FieldAccessor& operator=(const FieldAccessor& dest);

        private:
            FieldFuncTuple* functions_;
            const char* field_name_;
            const char* field_type_name_;

            explicit FieldAccessor(FieldFuncTuple* functions);
        };

        class MethodAccessor {
            friend class TypeMeta;
        public:
            MethodAccessor();

            void invoke(void* instance);
            const char* get_method_name() const;
            MethodAccessor& operator=(const MethodAccessor& dest);

        private:
            MethodFuncTuple* functions_;
            const char* method_name_;

            explicit MethodAccessor(MethodFuncTuple* functions);
        };

        class ArrayAccessor {
            friend class TypeMeta;
        public:
            ArrayAccessor();

            const char* get_array_type_name();
            const char* get_element_type_name();
            void set(int index, void* instance, void* element_value);
            void* get(int index, void* instance);
            int get_size(void* instance);
            ArrayAccessor& operator=(ArrayAccessor& dest);

        private:
            ArrayFuncTuple* functions_;
            const char* array_type_name_;
            const char* element_type_name_;

            explicit ArrayAccessor(ArrayFuncTuple* functions);
        };

        class ReflectionInstance {
        public:
            TypeMeta type_meta;
            void* instance;

            ReflectionInstance(TypeMeta meta, void* ins) : type_meta(meta), instance(ins) {}
            ReflectionInstance() : type_meta(), instance(nullptr) {}

            ReflectionInstance& operator=(ReflectionInstance& dest);
            ReflectionInstance& operator=(ReflectionInstance&& dest);
        };
    }

    template<typename T>
    class ReflectionPtr
        {
            template<typename U>
            friend class ReflectionPtr;

        public:
            ReflectionPtr(std::string type_name, T* instance) : m_type_name(type_name), m_instance(instance) {}
            ReflectionPtr() : m_type_name(), m_instance(nullptr) {}

            ReflectionPtr(const ReflectionPtr& dest) : m_type_name(dest.m_type_name), m_instance(dest.m_instance) {}

            template<typename U /*, typename = typename std::enable_if<std::is_safely_castable<T*, U*>::value>::type */>
            ReflectionPtr<T>& operator=(const ReflectionPtr<U>& dest)
            {
                if (this == static_cast<void*>(&dest))
                {
                    return *this;
                }
                m_type_name = dest.m_type_name;
                m_instance  = static_cast<T*>(dest.m_instance);
                return *this;
            }

            template<typename U /*, typename = typename std::enable_if<std::is_safely_castable<T*, U*>::value>::type*/>
            ReflectionPtr<T>& operator=(ReflectionPtr<U>&& dest)
            {
                if (this == static_cast<void*>(&dest))
                {
                    return *this;
                }
                m_type_name = dest.m_type_name;
                m_instance  = static_cast<T*>(dest.m_instance);
                return *this;
            }

            ReflectionPtr<T>& operator=(const ReflectionPtr<T>& dest)
            {
                if (this == &dest)
                {
                    return *this;
                }
                m_type_name = dest.m_type_name;
                m_instance  = dest.m_instance;
                return *this;
            }

            ReflectionPtr<T>& operator=(ReflectionPtr<T>&& dest)
            {
                if (this == &dest)
                {
                    return *this;
                }
                m_type_name = dest.m_type_name;
                m_instance  = dest.m_instance;
                return *this;
            }

            std::string getTypeName() const { return m_type_name; }

            void setTypeName(std::string name) { m_type_name = name; }

            bool operator==(const T* ptr) const { return (m_instance == ptr); }

            bool operator!=(const T* ptr) const { return (m_instance != ptr); }

            bool operator==(const ReflectionPtr<T>& rhs_ptr) const { return (m_instance == rhs_ptr.m_instance); }

            bool operator!=(const ReflectionPtr<T>& rhs_ptr) const { return (m_instance != rhs_ptr.m_instance); }

            template<
                typename T1 /*, typename = typename std::enable_if<std::is_safely_castable<T*, T1*>::value>::type*/>
            explicit operator T1*()
            {
                return static_cast<T1*>(m_instance);
            }

            template<
                typename T1 /*, typename = typename std::enable_if<std::is_safely_castable<T*, T1*>::value>::type*/>
            operator ReflectionPtr<T1>()
            {
                return ReflectionPtr<T1>(m_type_name, (T1*)(m_instance));
            }

            template<
                typename T1 /*, typename = typename std::enable_if<std::is_safely_castable<T*, T1*>::value>::type*/>
            explicit operator const T1*() const
            {
                return static_cast<T1*>(m_instance);
            }

            template<
                typename T1 /*, typename = typename std::enable_if<std::is_safely_castable<T*, T1*>::value>::type*/>
            operator const ReflectionPtr<T1>() const
            {
                return ReflectionPtr<T1>(m_type_name, (T1*)(m_instance));
            }

            T* operator->() { return m_instance; }

            T* operator->() const { return m_instance; }

            T& operator*() { return *(m_instance); }

            T* getPtr() { return m_instance; }

            T* getPtr() const { return m_instance; }

            const T& operator*() const { return *(static_cast<const T*>(m_instance)); }

            T*& getPtrReference() { return m_instance; }

            operator bool() const { return (m_instance != nullptr); }

        private:
            std::string m_type_name {""};
            typedef T   m_type;
            T*          m_instance {nullptr};
        };

}

#endif//DODOE_REFLECTION_HPP

