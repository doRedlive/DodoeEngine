#include "generator/script_binding_generator.h"
#include "common/precompiled.h"
#include "language_types/class.h"

namespace Generator
{
    namespace {

        struct FieldBindingInfo {
            std::string field_name;
            std::string field_name_cs;
            std::string field_type_cpp;
            std::string field_type_cs;
            std::string cs_proxy_getter;
            std::string cs_proxy_setter;
            std::string cs_nc_method_get;
            std::string cs_nc_method_set;
            std::string cs_nc_ret_type;
            std::string cs_nc_setter_param;
            std::string cpp_ret_type;
            std::string cpp_get_params;
            std::string cpp_set_params;
            std::string cpp_func_get;
            std::string cpp_func_set;
            std::string cpp_getter_body;
            std::string cpp_setter_body;
            // FOR_EACH_NATIVE_BINDING entries
            std::string cpp_bind_get_name, cpp_bind_set_name;
            std::string cpp_bind_get_ret, cpp_bind_set_ret;
            std::string cpp_bind_get_sig, cpp_bind_set_sig;
            std::string cpp_bind_get_invoke, cpp_bind_set_invoke;
            // C# NativeBindings struct field types
            std::string cs_bind_get_type, cs_bind_set_type;
        };

        static std::string snakeToPascal(const std::string& snake)
        {
            std::string result;
            bool upper = true;
            for (size_t i = 0; i < snake.size(); ++i) {
                char c = snake[i];
                if (c == '_') { upper = true; continue; }
                result += upper ? (char)toupper(c) : (char)tolower(c);
                upper = false;
            }
            return result;
        }

        static std::string resolveCsType(const std::string& cppType, std::string& outCppType)
        {
            std::string t = cppType;
            if (t.find("const ") == 0) t = t.substr(6);
            while (!t.empty() && t.back() == '&') t.pop_back();
            while (!t.empty() && t.back() == ' ') t.pop_back();
            outCppType = t;

            if (t.find("PPtr<") == 0) {
                size_t s = 5, e = t.rfind('>');
                if (e != std::string::npos && e > s)
                    return t.substr(s, e - s);
                return "";
            }
            if (t.find('<') != std::string::npos) return "";

            if (t == "float")  return "float";
            if (t == "int" || t == "int32_t" || t == "Int32")  return "int";
            if (t == "uint32_t" || t == "UInt32" || t == "unsigned int") return "uint";
            if (t == "uint64_t" || t == "UInt64" || t == "unsigned long long") return "ulong";
            if (t == "bool" || t == "Bool") return "bool";
            if (t == "Vector2i") return "Vector2i";
            if (t == "Vector2f") return "Vector2f";
            if (t == "Vector3i") return "Vector3i";
            if (t == "Vector3f") return "Vector3f";
            if (t == "Vector4i") return "Vector4i";
            if (t == "Vector4f") return "Vector4f";
            if (t == "Color") return "Color";
            if (t == "std::string" || t == "String") return "string";
            if (t == "Float")  return "float";
            if (t == "UUID" || t == "UUID") return "ulong";
            return "";
        }

        static bool isValueCsType(const std::string& t)
        {
            return t == "float" || t == "int" || t == "uint" || t == "ulong" || t == "bool";
        }

        static bool isStructCsType(const std::string& t)
        {
            return t == "Vector2i" || t == "Vector2f" ||
                   t == "Vector3i" || t == "Vector3f" ||
                   t == "Vector4i" || t == "Vector4f" ||
                   t == "Color";
        }

        static std::string nativeFuncName(const std::string& comp, const std::string& field)
        {
            return "native_" + comp + "_" + field;
        }
        static std::string cppFuncName(const std::string& comp, const std::string& field, const std::string& suffix)
        {
            return nativeFuncName(comp, field) + "_" + suffix;
        }

        static bool isObjectType(const std::string& t) { return t.find("PPtr<") == 0; }

        static bool hasDirtyField(const std::shared_ptr<Class>& class_temp)
        {
            for (const auto& field : class_temp->m_fields) {
                if (field && field->m_name == "dirty") {
                    return true;
                }
            }
            return false;
        }

        static bool hasDirtyField(const Class* class_temp)
        {
            if (!class_temp) return false;
            for (const auto& field : class_temp->m_fields) {
                if (field && field->m_name == "dirty") {
                    return true;
                }
            }
            return false;
        }

        static void buildCsProxyBodies(const std::string& compName, const std::string& csField, const std::string& csType, FieldBindingInfo& info)
        {
            std::string callPrefix = "NativeCalls." + compName + "_" + csField;
            if (isObjectType(info.field_type_cpp)) {
                info.cs_proxy_getter = "return Object.FindObjectFromInstanceID<" + csType + ">(" + callPrefix + "_Get(Entity.ID))!;";
                info.cs_proxy_setter = callPrefix + "_Set(Entity.ID, value?.InstanceID ?? 0);";
            } else if (isValueCsType(csType) || csType == "string") {
                info.cs_proxy_getter = "return " + callPrefix + "_Get(Entity.ID);";
                info.cs_proxy_setter = callPrefix + "_Set(Entity.ID, value);";
            } else {
                info.cs_proxy_getter = "return " + callPrefix + "_Get(Entity.ID);";
                info.cs_proxy_setter = callPrefix + "_Set(Entity.ID, ref value);";
            }
        }

        // Build C# NativeCalls method bodies (call b->native_xxx_get/set function pointers)
        static void buildCsNativeCallBodies(const std::string& compName, const std::string& csField, const std::string& csType, FieldBindingInfo& info)
        {
            std::string fn = nativeFuncName(compName, csField);

            if (isObjectType(info.field_type_cpp)) {
                info.cs_nc_ret_type = "int";
                info.cs_nc_setter_param = "int v";
                info.cs_nc_method_get = "return b->" + fn + "_get(entityId);";
                info.cs_nc_method_set = "b->" + fn + "_set(entityId, v);";
            } else if (isValueCsType(csType)) {
                info.cs_nc_ret_type = csType;
                info.cs_nc_setter_param = csType + " v";
                info.cs_nc_method_get = "return b->" + fn + "_get(entityId);";
                info.cs_nc_method_set = "b->" + fn + "_set(entityId, v);";
            } else if (isStructCsType(csType)) {
                info.cs_nc_ret_type = csType;
                if (csType == "Vector2i") {
                    info.cs_nc_setter_param = "ref Vector2i v";
                    info.cs_nc_method_get = "int x, y; b->" + fn + "_get(entityId, &x, &y); return new Vector2i(x, y);";
                    info.cs_nc_method_set = "b->" + fn + "_set(entityId, v.x, v.y);";
                } else if (csType == "Vector2f") {
                    info.cs_nc_setter_param = "ref Vector2f v";
                    info.cs_nc_method_get = "float x, y; b->" + fn + "_get(entityId, &x, &y); return new Vector2f(x, y);";
                    info.cs_nc_method_set = "b->" + fn + "_set(entityId, v.x, v.y);";
                } else if (csType == "Vector3i") {
                    info.cs_nc_setter_param = "ref Vector3i v";
                    info.cs_nc_method_get = "int x, y, z; b->" + fn + "_get(entityId, &x, &y, &z); return new Vector3i(x, y, z);";
                    info.cs_nc_method_set = "b->" + fn + "_set(entityId, v.x, v.y, v.z);";
                } else if (csType == "Vector3f") {
                    info.cs_nc_setter_param = "ref Vector3f v";
                    info.cs_nc_method_get = "float x, y, z; b->" + fn + "_get(entityId, &x, &y, &z); return new Vector3f(x, y, z);";
                    info.cs_nc_method_set = "b->" + fn + "_set(entityId, v.x, v.y, v.z);";
                } else if (csType == "Vector4i") {
                    info.cs_nc_setter_param = "ref Vector4i v";
                    info.cs_nc_method_get = "int x, y, z, w; b->" + fn + "_get(entityId, &x, &y, &z, &w); return new Vector4i(x, y, z, w);";
                    info.cs_nc_method_set = "b->" + fn + "_set(entityId, v.x, v.y, v.z, v.w);";
                } else if (csType == "Vector4f") {
                    info.cs_nc_setter_param = "ref Vector4f v";
                    info.cs_nc_method_get = "float x, y, z, w; b->" + fn + "_get(entityId, &x, &y, &z, &w); return new Vector4f(x, y, z, w);";
                    info.cs_nc_method_set = "b->" + fn + "_set(entityId, v.x, v.y, v.z, v.w);";
                } else {
                    info.cs_nc_setter_param = "ref Color v";
                    info.cs_nc_method_get = "float r, g, bl, a; b->" + fn + "_get(entityId, &r, &g, &bl, &a); return new Color(r, g, bl, a);";
                    info.cs_nc_method_set = "b->" + fn + "_set(entityId, v.r, v.g, v.b, v.a);";
                }
            } else if (csType == "string") {
                info.cs_nc_ret_type = "string";
                info.cs_nc_setter_param = "string v";
                info.cs_nc_method_get = "return PtrToStr(b->" + fn + "_get(entityId));";
                info.cs_nc_method_set = "var ptr = StrToPtr(v); try { b->" + fn + "_set(entityId, ptr); } finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }";
            } else {
                info.cs_nc_ret_type = csType;
                info.cs_nc_setter_param = csType + " v";
                info.cs_nc_method_get = "return b->" + fn + "_get(entityId);";
                info.cs_nc_method_set = "b->" + fn + "_set(entityId, v);";
            }
        }

        static void buildCppBodies(const std::string& compName, const std::string& fieldName, const std::string& fieldNameCs, const std::string& csType, FieldBindingInfo& info, bool hasGetter = false, bool hasSetter = false, bool markDirty = false)
        {
            std::string fnGet = cppFuncName(compName, fieldName, "get");
            std::string fnSet = cppFuncName(compName, fieldName, "set");
            info.cpp_func_get = fnGet;
            info.cpp_func_set = fnSet;

            if (isObjectType(info.field_type_cpp)) {
                info.cpp_ret_type = "int";
                info.cpp_get_params = "";
                info.cpp_set_params = "int v";
                if (info.field_type_cpp == "PPtr<Texture>") {
                    info.cpp_getter_body =
                        "if (auto* c = TryGetComponent<" + compName + ">(uuid)) { "
                        "if (auto* obj = c->" + fieldName + ".get()) return (int)obj->getInstanceID(); "
                        "const auto& file_id = c->" + fieldName + ".getFileID(); "
                        "if (file_id.isValid()) { if (auto obj = Texture2D::Load(file_id.getPath())) return (int)obj->getInstanceID(); } "
                        "} return 0;";
                } else {
                    info.cpp_getter_body =
                        "if (auto* c = TryGetComponent<" + compName + ">(uuid)) { "
                        "if (auto* obj = c->" + fieldName + ".get()) return (int)obj->getInstanceID(); "
                        "} return 0;";
                }
                const std::string dirtyAssign = markDirty ? "        c->dirty = true;\n" : "";
                info.cpp_setter_body =
                    "if (auto* c = TryGetComponent<" + compName + ">(uuid)) {\n"
                    "    if (v == 0) {\n"
                    "        c->" + fieldName + " = {};\n" +
                    dirtyAssign +
                    "        return;\n"
                    "    }\n"
                    "    if (auto* obj = Object::FindObjectFromInstanceID((InstanceID)v)) {\n"
                    "        c->" + fieldName + " = " + info.field_type_cpp + "(obj->getFileID(), obj->getUUID(), (InstanceID)v);\n" +
                    dirtyAssign +
                    "        return;\n"
                    "    }\n"
                    "    DO_ERROR(\"" + compName + "." + fieldName + ": invalid object instance id {}\", v);\n"
                    "    c->" + fieldName + " = {};\n" +
                    dirtyAssign +
                    "}";
            } else if (info.field_type_cpp == "UUID" || info.field_type_cpp == "UUID") {
                info.cpp_ret_type = "uint64_t";
                info.cpp_get_params = "";
                info.cpp_set_params = "uint64_t v";
                if (hasGetter)
                    info.cpp_getter_body = "if (auto* c = TryGetComponent<" + compName + ">(uuid)) return (uint64_t)c->get" + fieldNameCs + "(); return 0;";
                else
                    info.cpp_getter_body = "if (auto* c = TryGetComponent<" + compName + ">(uuid)) return (uint64_t)c->" + fieldName + "; return 0;";
                if (hasSetter)
                    info.cpp_setter_body = "if (auto* c = TryGetComponent<" + compName + ">(uuid)) c->set" + fieldNameCs + "(UUID(v));";
                else
                    info.cpp_setter_body = "if (auto* c = TryGetComponent<" + compName + ">(uuid)) c->" + fieldName + " = UUID(v);";
            } else if (isValueCsType(csType)) {
                info.cpp_ret_type = csType;
                info.cpp_get_params = "";
                info.cpp_set_params = csType + " v";
                if (hasGetter)
                    info.cpp_getter_body = "if (auto* c = TryGetComponent<" + compName + ">(uuid)) return c->get" + fieldNameCs + "(); return {};";
                else
                    info.cpp_getter_body = "if (auto* c = TryGetComponent<" + compName + ">(uuid)) return c->" + fieldName + "; return {};";
                if (hasSetter)
                    info.cpp_setter_body = "if (auto* c = TryGetComponent<" + compName + ">(uuid)) c->set" + fieldNameCs + "(v);";
                else
                    info.cpp_setter_body = "if (auto* c = TryGetComponent<" + compName + ">(uuid)) c->" + fieldName + " = v;";
            } else if (isStructCsType(csType)) {
                info.cpp_ret_type = "void";
                std::string comps, zero, getAssign, setAssign;
                if (csType == "Vector2i") {
                    comps = "x, y";
                    info.cpp_get_params = "int* x, int* y";
                    info.cpp_set_params = "int x, int y";
                    zero = "if (x) *x = 0; if (y) *y = 0;";
                    if (hasGetter) {
                        getAssign = "auto v = c->get" + fieldNameCs + "(); if (x) *x = v.x; if (y) *y = v.y;";
                    } else {
                        getAssign = "auto v = c->" + fieldName + "; if (x) *x = v.x; if (y) *y = v.y;";
                    }
                    setAssign = hasSetter ? ("c->set" + fieldNameCs + "({x, y});") : ("c->" + fieldName + " = {x, y};");
                } else if (csType == "Vector2f") {
                    comps = "x, y";
                    info.cpp_get_params = "float* x, float* y";
                    info.cpp_set_params = "float x, float y";
                    zero = "if (x) *x = 0; if (y) *y = 0;";
                    if (hasGetter) {
                        getAssign = "auto v = c->get" + fieldNameCs + "(); if (x) *x = v.x; if (y) *y = v.y;";
                    } else {
                        getAssign = "auto v = c->" + fieldName + "; if (x) *x = v.x; if (y) *y = v.y;";
                    }
                    setAssign = hasSetter ? ("c->set" + fieldNameCs + "({x, y});") : ("c->" + fieldName + " = {x, y};");
                } else if (csType == "Vector3i") {
                    comps = "x, y, z";
                    info.cpp_get_params = "int* x, int* y, int* z";
                    info.cpp_set_params = "int x, int y, int z";
                    zero = "if (x) *x = 0; if (y) *y = 0; if (z) *z = 0;";
                    if (hasGetter) {
                        getAssign = "auto v = c->get" + fieldNameCs + "(); if (x) *x = v.x; if (y) *y = v.y; if (z) *z = v.z;";
                    } else {
                        getAssign = "auto v = c->" + fieldName + "; if (x) *x = v.x; if (y) *y = v.y; if (z) *z = v.z;";
                    }
                    setAssign = hasSetter ? ("c->set" + fieldNameCs + "({x, y, z});") : ("c->" + fieldName + " = {x, y, z};");
                } else if (csType == "Vector3f") {
                    comps = "x, y, z";
                    info.cpp_get_params = "float* x, float* y, float* z";
                    info.cpp_set_params = "float x, float y, float z";
                    zero = "if (x) *x = 0; if (y) *y = 0; if (z) *z = 0;";
                    if (hasGetter) {
                        getAssign = "auto v = c->get" + fieldNameCs + "(); if (x) *x = v.x; if (y) *y = v.y; if (z) *z = v.z;";
                    } else {
                        getAssign = "auto v = c->" + fieldName + "; if (x) *x = v.x; if (y) *y = v.y; if (z) *z = v.z;";
                    }
                    setAssign = hasSetter ? ("c->set" + fieldNameCs + "({x, y, z});") : ("c->" + fieldName + " = {x, y, z};");
                } else if (csType == "Vector4i") {
                    info.cpp_get_params = "int* x, int* y, int* z, int* w";
                    info.cpp_set_params = "int x, int y, int z, int w";
                    zero = "if (x) *x = 0; if (y) *y = 0; if (z) *z = 0; if (w) *w = 0;";
                    if (hasGetter) {
                        getAssign = "auto v = c->get" + fieldNameCs + "(); if (x) *x = v.x; if (y) *y = v.y; if (z) *z = v.z; if (w) *w = v.w;";
                    } else {
                        getAssign = "auto v = c->" + fieldName + "; if (x) *x = v.x; if (y) *y = v.y; if (z) *z = v.z; if (w) *w = v.w;";
                    }
                    setAssign = hasSetter ? ("c->set" + fieldNameCs + "({x, y, z, w});") : ("c->" + fieldName + " = {x, y, z, w};");
                } else if (csType == "Vector4f") {
                    info.cpp_get_params = "float* x, float* y, float* z, float* w";
                    info.cpp_set_params = "float x, float y, float z, float w";
                    zero = "if (x) *x = 0; if (y) *y = 0; if (z) *z = 0; if (w) *w = 0;";
                    if (hasGetter) {
                        getAssign = "auto v = c->get" + fieldNameCs + "(); if (x) *x = v.x; if (y) *y = v.y; if (z) *z = v.z; if (w) *w = v.w;";
                    } else {
                        getAssign = "auto v = c->" + fieldName + "; if (x) *x = v.x; if (y) *y = v.y; if (z) *z = v.z; if (w) *w = v.w;";
                    }
                    setAssign = hasSetter ? ("c->set" + fieldNameCs + "({x, y, z, w});") : ("c->" + fieldName + " = {x, y, z, w};");
                } else {
                    info.cpp_get_params = "float* r, float* g, float* b, float* a";
                    info.cpp_set_params = "float r, float g, float b, float a";
                    zero = "if (r) *r = 1; if (g) *g = 1; if (b) *b = 1; if (a) *a = 1;";
                    if (hasGetter) {
                        getAssign = "auto v = c->get" + fieldNameCs + "(); if (r) *r = v.r; if (g) *g = v.g; if (b) *b = v.b; if (a) *a = v.a;";
                    } else {
                        getAssign = "auto v = c->" + fieldName + "; if (r) *r = v.r; if (g) *g = v.g; if (b) *b = v.b; if (a) *a = v.a;";
                    }
                    setAssign = hasSetter ? ("c->set" + fieldNameCs + "({r, g, b, a});") : ("c->" + fieldName + " = {r, g, b, a};");
                }
                info.cpp_getter_body = zero + " if (auto* c = TryGetComponent<" + compName + ">(uuid)) { " + getAssign + " }";
                info.cpp_setter_body = "if (auto* c = TryGetComponent<" + compName + ">(uuid)) " + setAssign;
            } else if (csType == "string") {
                info.cpp_ret_type = "const char*";
                info.cpp_get_params = "";
                info.cpp_set_params = "const char* v";
                std::string tag = compName + "_" + fieldName;
                if (hasGetter)
                    info.cpp_getter_body = "DEF_STR_RET(" + tag + "); auto* c = TryGetComponent<" + compName + ">(uuid); if (c) { _s_" + tag + " = c->get" + fieldNameCs + "(); return _s_" + tag + ".c_str(); } return \"\";";
                else
                    info.cpp_getter_body = "DEF_STR_RET(" + tag + "); auto* c = TryGetComponent<" + compName + ">(uuid); if (c) { _s_" + tag + " = c->" + fieldName + "; return _s_" + tag + ".c_str(); } return \"\";";
                if (hasSetter)
                    info.cpp_setter_body = "if (auto* c = TryGetComponent<" + compName + ">(uuid)) { if (v) c->set" + fieldNameCs + "(v); }";
                else
                    info.cpp_setter_body = "if (auto* c = TryGetComponent<" + compName + ">(uuid)) { if (v) c->" + fieldName + " = v; }";
            } else {
                info.cpp_ret_type = csType;
                info.cpp_get_params = "";
                info.cpp_set_params = csType + " v";
                if (hasGetter)
                    info.cpp_getter_body = "if (auto* c = TryGetComponent<" + compName + ">(uuid)) return c->get" + fieldNameCs + "(); return {};";
                else
                    info.cpp_getter_body = "if (auto* c = TryGetComponent<" + compName + ">(uuid)) return c->" + fieldName + "; return {};";
                if (hasSetter)
                    info.cpp_setter_body = "if (auto* c = TryGetComponent<" + compName + ">(uuid)) c->set" + fieldNameCs + "(v);";
                else
                    info.cpp_setter_body = "if (auto* c = TryGetComponent<" + compName + ">(uuid)) c->" + fieldName + " = v;";
            }
        }

        static void buildBindingsEntries(const std::string& compName, const std::string& fieldName, const std::string& csType, FieldBindingInfo& info)
        {
            std::string nativeGet = nativeFuncName(compName, fieldName) + "_get";
            std::string nativeSet = nativeFuncName(compName, fieldName) + "_set";

            info.cpp_bind_get_name = nativeGet;
            info.cpp_bind_set_name = nativeSet;

            bool isUuid = (info.field_type_cpp == "UUID" || info.field_type_cpp == "UUID");
            std::string bindCsType = isObjectType(info.field_type_cpp) ? "int" : (isUuid ? "uint64_t" : csType);

            // C# delegate type mapper
            auto csParamType = [](const std::string& cpp) -> std::string {
                if (cpp == "float") return "float";
                if (cpp == "float*") return "float*";
                if (cpp == "int" || cpp == "int32_t") return "int";
                if (cpp == "uint32_t" || cpp == "uint") return "uint";
                if (cpp == "uint64_t" || cpp == "ulong") return "ulong";
                if (cpp == "bool") return "bool";
                if (cpp == "const char*") return "byte*";
                return "int";
            };

            if (isObjectType(info.field_type_cpp))
            {
                info.cpp_bind_get_ret = "int";
                info.cpp_bind_get_sig = "(uint64_t e)";
                info.cpp_bind_get_invoke = "e";
                info.cs_bind_get_type = "delegate* unmanaged<ulong, int>";

                info.cpp_bind_set_ret = "void";
                info.cpp_bind_set_sig = "(uint64_t e, int v)";
                info.cpp_bind_set_invoke = "e, v";
                info.cs_bind_set_type = "delegate* unmanaged<ulong, int, void>";
            }
            else if (isValueCsType(csType))
            {
                info.cpp_bind_get_ret = bindCsType;
                info.cpp_bind_get_sig = "(uint64_t e)";
                info.cpp_bind_get_invoke = "e";
                info.cs_bind_get_type = "delegate* unmanaged<ulong, " + csType + ">";

                info.cpp_bind_set_ret = "void";
                info.cpp_bind_set_sig = "(uint64_t e, " + bindCsType + " v)";
                info.cpp_bind_set_invoke = "e, v";
                info.cs_bind_set_type = "delegate* unmanaged<ulong, " + csType + ", void>";
            }
            else if (isStructCsType(csType))
            {
                info.cpp_bind_get_ret = "void";
                info.cpp_bind_set_ret = "void";

                if (csType == "Vector2i") {
                    info.cpp_bind_get_sig = "(uint64_t e, int* x, int* y)";
                    info.cpp_bind_get_invoke = "e, x, y";
                    info.cs_bind_get_type = "delegate* unmanaged<ulong, int*, int*, void>";
                    info.cpp_bind_set_sig = "(uint64_t e, int x, int y)";
                    info.cpp_bind_set_invoke = "e, x, y";
                    info.cs_bind_set_type = "delegate* unmanaged<ulong, int, int, void>";
                } else if (csType == "Vector2f") {
                    info.cpp_bind_get_sig = "(uint64_t e, float* x, float* y)";
                    info.cpp_bind_get_invoke = "e, x, y";
                    info.cs_bind_get_type = "delegate* unmanaged<ulong, float*, float*, void>";
                    info.cpp_bind_set_sig = "(uint64_t e, float x, float y)";
                    info.cpp_bind_set_invoke = "e, x, y";
                    info.cs_bind_set_type = "delegate* unmanaged<ulong, float, float, void>";
                } else if (csType == "Vector3i") {
                    info.cpp_bind_get_sig = "(uint64_t e, int* x, int* y, int* z)";
                    info.cpp_bind_get_invoke = "e, x, y, z";
                    info.cs_bind_get_type = "delegate* unmanaged<ulong, int*, int*, int*, void>";
                    info.cpp_bind_set_sig = "(uint64_t e, int x, int y, int z)";
                    info.cpp_bind_set_invoke = "e, x, y, z";
                    info.cs_bind_set_type = "delegate* unmanaged<ulong, int, int, int, void>";
                } else if (csType == "Vector3f") {
                    info.cpp_bind_get_sig = "(uint64_t e, float* x, float* y, float* z)";
                    info.cpp_bind_get_invoke = "e, x, y, z";
                    info.cs_bind_get_type = "delegate* unmanaged<ulong, float*, float*, float*, void>";
                    info.cpp_bind_set_sig = "(uint64_t e, float x, float y, float z)";
                    info.cpp_bind_set_invoke = "e, x, y, z";
                    info.cs_bind_set_type = "delegate* unmanaged<ulong, float, float, float, void>";
                } else if (csType == "Vector4i") {
                    info.cpp_bind_get_sig = "(uint64_t e, int* x, int* y, int* z, int* w)";
                    info.cpp_bind_get_invoke = "e, x, y, z, w";
                    info.cs_bind_get_type = "delegate* unmanaged<ulong, int*, int*, int*, int*, void>";
                    info.cpp_bind_set_sig = "(uint64_t e, int x, int y, int z, int w)";
                    info.cpp_bind_set_invoke = "e, x, y, z, w";
                    info.cs_bind_set_type = "delegate* unmanaged<ulong, int, int, int, int, void>";
                } else { // Vector4f / Color
                    info.cpp_bind_get_sig = "(uint64_t e, float* r, float* g, float* b, float* a)";
                    info.cpp_bind_get_invoke = "e, r, g, b, a";
                    info.cs_bind_get_type = "delegate* unmanaged<ulong, float*, float*, float*, float*, void>";
                    info.cpp_bind_set_sig = "(uint64_t e, float r, float g, float b, float a)";
                    info.cpp_bind_set_invoke = "e, r, g, b, a";
                    info.cs_bind_set_type = "delegate* unmanaged<ulong, float, float, float, float, void>";
                }
            }
            else if (csType == "string")
            {
                info.cpp_bind_get_ret = "const char*";
                info.cpp_bind_get_sig = "(uint64_t e)";
                info.cpp_bind_get_invoke = "e";
                info.cs_bind_get_type = "delegate* unmanaged<ulong, byte*>";

                info.cpp_bind_set_ret = "void";
                info.cpp_bind_set_sig = "(uint64_t e, const char* v)";
                info.cpp_bind_set_invoke = "e, v";
                info.cs_bind_set_type = "delegate* unmanaged<ulong, byte*, void>";
            }
            else
            {
                info.cpp_bind_get_ret = csType;
                info.cpp_bind_get_sig = "(uint64_t e)";
                info.cpp_bind_get_invoke = "e";
                info.cs_bind_get_type = "delegate* unmanaged<ulong, " + csType + ">";

                info.cpp_bind_set_ret = "void";
                info.cpp_bind_set_sig = "(uint64_t e, " + csType + " v)";
                info.cpp_bind_set_invoke = "e, v";
                info.cs_bind_set_type = "delegate* unmanaged<ulong, " + csType + ", void>";
            }
        }

        static bool buildBinding(const std::string& className, std::shared_ptr<Field> field, FieldBindingInfo& info)
        {
            info.field_name = field->m_name;
            info.field_name_cs = snakeToPascal(field->m_name);
            info.field_type_cs = resolveCsType(field->m_type, info.field_type_cpp);
            if (info.field_type_cs.empty()) return false;

            bool hasGetter = false;
            bool hasSetter = false;
            if (field->m_parent) {
                std::string getterName = "get" + info.field_name_cs;
                std::string setterName = "set" + info.field_name_cs;
                for (auto& method : field->m_parent->m_methods) {
                    if (method->m_name == getterName) hasGetter = true;
                    if (method->m_name == setterName) hasSetter = true;
                }
            }

            buildCsProxyBodies(className, info.field_name_cs, info.field_type_cs, info);
            buildCsNativeCallBodies(className, info.field_name, info.field_type_cs, info);
            buildCppBodies(className, info.field_name, info.field_name_cs, info.field_type_cs, info, hasGetter, hasSetter, hasDirtyField(field->m_parent));
            buildBindingsEntries(className, info.field_name, info.field_type_cs, info);
            return true;
        }

    } // namespace

    ScriptBindingGenerator::ScriptBindingGenerator(std::string source_directory,
                                                   std::function<std::string(std::string)> get_include_function) :
        GeneratorInterface(source_directory + "/_generated/script", source_directory, get_include_function)
    {
        prepareStatus(m_out_path);
    }

    void ScriptBindingGenerator::prepareStatus(std::string path)
    {
        GeneratorInterface::prepareStatus(path);
        TemplateManager::getInstance()->loadTemplates(m_root_path, "NativeComponentsFile");
        TemplateManager::getInstance()->loadTemplates(m_root_path, "NativeCallsFile");
        TemplateManager::getInstance()->loadTemplates(m_root_path, "ScriptGlueFile");
        TemplateManager::getInstance()->loadTemplates(m_root_path, "PyBindingsFile");
        TemplateManager::getInstance()->loadTemplates(m_root_path, "NativeBindingsCppFile");
        TemplateManager::getInstance()->loadTemplates(m_root_path, "NativeBindingsCsFile");
    }

    std::string ScriptBindingGenerator::processFileName(std::string path) { return ""; }

    int ScriptBindingGenerator::generate(std::string path, SchemaMoudle schema)
    {
        for (auto class_temp : schema.classes)
        {
            if (!class_temp->shouldCompileFields()) continue;
            if (!class_temp->shouldScriptBind()) continue;

            Mustache::data field_list(Mustache::data::type::list);
            Mustache::data cpp_field_list(Mustache::data::type::list);
            int count = 0;

            for (auto field : class_temp->m_fields)
            {
                if (!field->shouldCompile()) continue;

                FieldBindingInfo info;
                if (!buildBinding(class_temp->getClassName(), field, info)) continue;
                ++count;

                // NativeComponents.generated.cs — proxy class fields
                Mustache::data fd;
                fd.set("field_name", info.field_name);
                fd.set("field_name_cs", info.field_name_cs);
                fd.set("field_type_cs", info.field_type_cs);
                fd.set("getter_body", info.cs_proxy_getter);
                fd.set("setter_body", info.cs_proxy_setter);
                field_list.push_back(fd);

                // NativeCalls.generated.cs — one wrapper method pair per field
                Mustache::data nc;
                nc.set("class_name", class_temp->getClassName());
                nc.set("field_name_cs", info.field_name_cs);
                nc.set("field_type_cs", info.field_type_cs);
                nc.set("nc_ret_type", info.cs_nc_ret_type);
                nc.set("nc_setter_param", info.cs_nc_setter_param);
                nc.set("nc_getter_body", info.cs_nc_method_get);
                nc.set("nc_setter_body", info.cs_nc_method_set);
                m_cs_nativecalls_defines.push_back(nc);

                // script_glue.generated.cpp
                Mustache::data cpp;
                cpp.set("class_name", class_temp->getClassName());
                cpp.set("field_name", info.field_name);
                cpp.set("cpp_ret_type", info.cpp_ret_type);
                cpp.set("cpp_func_get", info.cpp_func_get);
                cpp.set("cpp_func_set", info.cpp_func_set);
                cpp.set("cpp_get_params", info.cpp_get_params);
                cpp.set("cpp_set_params", info.cpp_set_params);
                cpp.set("has_get_params", !info.cpp_get_params.empty());
                cpp.set("has_set_params", !info.cpp_set_params.empty());
                cpp.set("getter_body", info.cpp_getter_body);
                cpp.set("setter_body", info.cpp_setter_body);
                cpp_field_list.push_back(cpp);

                // C++ FOR_EACH_NATIVE_BINDING entry (getter)
                Mustache::data bg;
                bg.set("bind_name", info.cpp_bind_get_name);
                bg.set("bind_ret", info.cpp_bind_get_ret);
                bg.set("bind_sig", info.cpp_bind_get_sig);
                bg.set("bind_invoke", info.cpp_bind_get_invoke);
                m_native_bindings_cpp.push_back(bg);

                // C++ FOR_EACH_NATIVE_BINDING entry (setter)
                Mustache::data bs;
                bs.set("bind_name", info.cpp_bind_set_name);
                bs.set("bind_ret", info.cpp_bind_set_ret);
                bs.set("bind_sig", info.cpp_bind_set_sig);
                bs.set("bind_invoke", info.cpp_bind_set_invoke);
                m_native_bindings_cpp.push_back(bs);

                // C# NativeBindings struct field (getter)
                Mustache::data csbg;
                csbg.set("bind_type", info.cs_bind_get_type);
                csbg.set("bind_name", info.cpp_bind_get_name);
                m_native_bindings_cs.push_back(csbg);

                // C# NativeBindings struct field (setter)
                Mustache::data csbs;
                csbs.set("bind_type", info.cs_bind_set_type);
                csbs.set("bind_name", info.cpp_bind_set_name);
                m_native_bindings_cs.push_back(csbs);
            }

            if (count == 0) continue;

            Mustache::data class_entry;
            class_entry.set("class_name", class_temp->getClassName());
            class_entry.set("field_defines", field_list);
            m_cs_component_defines.push_back(class_entry);

            Mustache::data cpp_class;
            cpp_class.set("class_name", class_temp->getClassName());
            cpp_class.set("field_defines", cpp_field_list);
            m_cpp_glue_defines.push_back(cpp_class);

            Mustache::data py_class;
            py_class.set("class_name", class_temp->getClassName());
            py_class.set("field_defines", field_list);
            m_pybind_defines.push_back(py_class);
        }

        return 0;
    }

    void ScriptBindingGenerator::finish()
    {
        if (m_cs_component_defines.is_list() && !m_cs_component_defines.list_value().empty()) {
            Mustache::data data;
            data.set("class_defines", m_cs_component_defines);
            Utils::saveFile(TemplateManager::getInstance()->renderByTemplate("NativeComponentsFile", data),
                            m_out_path + "/NativeComponents.generated.cs");
        }

        if (m_cs_nativecalls_defines.is_list() && !m_cs_nativecalls_defines.list_value().empty()) {
            Mustache::data data;
            data.set("class_defines", m_cs_nativecalls_defines);
            Utils::saveFile(TemplateManager::getInstance()->renderByTemplate("NativeCallsFile", data),
                            m_out_path + "/NativeCalls.generated.cs");
        }

        if (m_cpp_glue_defines.is_list() && !m_cpp_glue_defines.list_value().empty()) {
            Mustache::data data;
            data.set("class_defines", m_cpp_glue_defines);
            Utils::saveFile(TemplateManager::getInstance()->renderByTemplate("ScriptGlueFile", data),
                            m_out_path + "/script_glue.generated.cpp");
        }

        if (m_pybind_defines.is_list() && !m_pybind_defines.list_value().empty()) {
            Mustache::data data;
            data.set("class_defines", m_pybind_defines);
            Utils::saveFile(TemplateManager::getInstance()->renderByTemplate("PyBindingsFile", data),
                            m_out_path + "/py_bindings_components.generated.cpp");
        }

        if (m_native_bindings_cpp.is_list() && !m_native_bindings_cpp.list_value().empty()) {
            Mustache::data data;
            data.set("binding_defines", m_native_bindings_cpp);
            Utils::saveFile(TemplateManager::getInstance()->renderByTemplate("NativeBindingsCppFile", data),
                            m_out_path + "/script_glue_bindings.generated.h");
        }

        if (m_native_bindings_cs.is_list() && !m_native_bindings_cs.list_value().empty()) {
            Mustache::data data;
            data.set("binding_defines", m_native_bindings_cs);
            Utils::saveFile(TemplateManager::getInstance()->renderByTemplate("NativeBindingsCsFile", data),
                            m_out_path + "/NativeBindings.generated.cs");
        }
    }

    ScriptBindingGenerator::~ScriptBindingGenerator() {}

} // namespace Generator
