// do@Redlive

#include "dopch.h"

#include <windows.h>

#include "script_engine.h"

#include "runtime/core/project/project.h"
#include "runtime/platform/platform_tool.h"

#include "mono/jit/jit.h"
#include "mono/jit/mono-private-unstable.h"
#include "mono/metadata/mono-private-unstable.h"
#include "mono/metadata/assembly.h"
#include "mono/metadata/appdomain.h"
#include "mono/metadata/class.h"
#include "mono/metadata/object.h"
#include "mono/metadata/threads.h"
#include "mono/utils/mono-publib.h"
#include "mono/metadata/mono-gc.h"

namespace dodoe {

    namespace fs = std::filesystem;

    namespace {

        Bool BuildScriptSourceFingerprint(const fs::path& asset_directory, String& out_fingerprint) {
            out_fingerprint.clear();
            if (!fs::exists(asset_directory) || !fs::is_directory(asset_directory)) {
                return false;
            }

            DynamicArray<fs::path> script_files;
            for (const auto& entry : fs::recursive_directory_iterator(asset_directory)) {
                if (!entry.is_regular_file() || entry.path().extension() != ".cs") {
                    continue;
                }
                script_files.push_back(entry.path().lexically_normal());
            }

            if (script_files.empty()) {
                return false;
            }

            std::ranges::sort(script_files);
            std::ostringstream stream;
            for (const auto& script_file : script_files) {
                std::error_code ec;
                const auto relative_path = fs::relative(script_file, asset_directory, ec);
                const auto last_write_time = fs::last_write_time(script_file, ec);
                const auto file_size = fs::file_size(script_file, ec);
                stream << (ec ? script_file.lexically_normal().generic_string() : relative_path.lexically_normal().generic_string())
                    << '|'
                    << (ec ? 0ull : static_cast<unsigned long long>(file_size))
                    << '|'
                    << (ec ? 0ll : static_cast<long long>(last_write_time.time_since_epoch().count()))
                    << '\n';
            }

            out_fingerprint = stream.str();
            return true;
        }

        fs::path GetExeDir() {
            DynamicArray<char> buffer(256, '\0');
            const DWORD len = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            return len ? fs::path(buffer.data()).parent_path() : fs::path();
        }

        String ResolveTpaList(const fs::path& dir) {
            DynamicArray<String> tpa_list;
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".dll") {
                    tpa_list.push_back(entry.path().string());
                }
            }
            std::sort(tpa_list.begin(), tpa_list.end());

            String tpa;
            for (size_t i = 0; i < tpa_list.size(); ++i) {
                if (i) tpa += ';';
                tpa += tpa_list[i];
            }

            return tpa;
        }

        DynamicArray<Byte> ReadFileBinary(const fs::path& filepath) {
            std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
            if (!stream.is_open()) {
                return {};
            }

            const auto size = stream.tellg();
            stream.seekg(0, std::ios::beg);

            DynamicArray<Byte> buffer(static_cast<Size_t>(size));
            stream.read(reinterpret_cast<char*>(buffer.data()), size);
            return buffer;
        }

        MonoAssembly* LoadAssemblyFromMemory(const DynamicArray<Byte>& data, const String& name) {
            if (data.empty()) {
                return nullptr;
            }

            MonoImageOpenStatus status;
            MonoImage* image = mono_image_open_from_data_full(
                const_cast<char*>(reinterpret_cast<const char*>(data.data())),
                static_cast<ui32>(data.size()),
                1, &status, 0);

            if (status != MONO_IMAGE_OK) {
                DO_ERROR("ScriptEngine: mono_image_open_from_data_full failed for '{}', status={}", name, static_cast<Int>(status));
                return nullptr;
            }

            MonoAssembly* assembly = mono_assembly_load_from_full(image, name.c_str(), &status, 0);
            mono_image_close(image);

            if (status != MONO_IMAGE_OK || !assembly) {
                DO_ERROR("ScriptEngine: mono_assembly_load_from_full failed for '{}', status={}", name, static_cast<Int>(status));
                return nullptr;
            }

            return assembly;
        }
    }

    Bool ScriptEngine::onScriptSourcesChanged() {
        const auto active_project = Project::ActiveProject();
        if (!active_project) {
            return false;
        }

        String script_sources_fingerprint;
        if (!BuildScriptSourceFingerprint(Project::AssetDirectory(), script_sources_fingerprint)) {
            m_script_sources_fingerprint.clear();
            return false;
        }

        if (m_app_image && script_sources_fingerprint == m_script_sources_fingerprint) {
            return false;
        }

        m_pending_fingerprint = std::move(script_sources_fingerprint);
        return true;
    }

    void ScriptEngine::commitScriptFingerprint() {
        m_script_sources_fingerprint = std::move(m_pending_fingerprint);
        m_pending_fingerprint.clear();
    }

    Bool ScriptEngine::initialize(const ScriptEngineCreateInfo& info) {
        if (!setupMono()) return false;
        if (!loadCoreAssembly("")) return false;
        if (!loadAppAssembly()) return false;
        return true;
    }

    void ScriptEngine::shutdown() {
        cleanupMono();
    }

    Bool ScriptEngine::buildAppAssembly() {
        const auto active_project = Project::ActiveProject();
        if (!active_project) {
            return false;
        }
        if (PlatformTool::BuildCSharpAssembly(Project::AssetDirectory(), Project::BinariesDirectory(), active_project->config().name)) {
            return true;
        }
        DO_ERROR("ScriptEngine build script assembly failed!");
        return false;
    }

    void ScriptEngine::resetManagedState() {
        if (!m_core_image) return;
        mono_domain_set(m_core_domain, true);

        MonoClass* ec = mono_class_from_name(m_core_image, "GreenCake", "ExternalCalls");
        if (!ec) { DO_ERROR("ScriptEngine: ExternalCalls not found"); return; }

        MonoMethod* reset = mono_class_get_method_from_name(ec, "Reset", 0);
        if (!reset) { DO_ERROR("ScriptEngine: ExternalCalls.Reset() not found"); return; }

        MonoObject* ex = nullptr;
        mono_runtime_invoke(reset, nullptr, nullptr, &ex);
        if (ex) DO_ERROR("ScriptEngine: ExternalCalls.Reset() threw exception");
    }

    Bool ScriptEngine::setupMono() {
        const fs::path runtime_dir =
            R"(C:\Users\33235\Redlive\Libraries\runtime\artifacts\bin\testhost\net8.0-windows-Release-x64\shared\Microsoft.NETCore.App\8.0.25)";
        const fs::path exe_dir = GetExeDir();

        const String tpa = ResolveTpaList(runtime_dir);
        const String runtime_dir_str = runtime_dir.string();
        const String exe_dir_str = exe_dir.string();
        const String app_paths = exe_dir_str + ";" + runtime_dir_str;
        const char* keys[] = {
            "RUNTIME_IDENTIFIER",
            "APP_CONTEXT_BASE_DIRECTORY",
            "APP_PATHS",
            "NATIVE_DLL_SEARCH_DIRECTORIES",
            "TRUSTED_PLATFORM_ASSEMBLIES"
        };
        const char* values[] = {
            "win-x64",
            exe_dir_str.c_str(),
            app_paths.c_str(),
            app_paths.c_str(),
            tpa.c_str()
        };

        DO_ASSERT(monovm_initialize(static_cast<Int>(std::size(keys)), keys, values) == 0, "monovm initialize failed");
        mono_set_assemblies_path(runtime_dir_str.c_str());

        m_root_domain = mono_jit_init("DodoeJitRuntime");
        DO_ASSERT(m_root_domain, "MonoJit init failed");

        mono_thread_set_main(mono_thread_current());

        return true;
    }

    Bool ScriptEngine::loadCoreAssembly(const String& path) {
        (void)path;
        m_core_domain = m_root_domain;
        mono_domain_set(m_core_domain, true);

        const fs::path assembly_path = GetExeDir() / "GreenCake.dll";
        auto data = ReadFileBinary(assembly_path);
        if (data.empty()) {
            DO_ASSERT(false, "ScriptEngine: failed to read GreenCake.dll");
            return false;
        }

        m_core_assembly = LoadAssemblyFromMemory(data, "GreenCake.dll");
        DO_ASSERT(m_core_assembly);

        m_core_image = mono_assembly_get_image(m_core_assembly);
        DO_ASSERT(m_core_image);

        return true;
    }

    void ScriptEngine::unloadAppAssembly() {
        if (!m_alc_gchandle) {
            m_app_image = nullptr;
            m_app_assembly = nullptr;
            return;
        }

        mono_domain_set(m_core_domain, true);

        if (m_app_image) {
            mono_image_close(m_app_image);
            m_app_image = nullptr;
        }
        m_app_assembly = nullptr;

        MonoObject* alc = mono_gchandle_get_target_v2(static_cast<MonoGCHandle>(m_alc_gchandle));
        if (alc) {
            MonoClass* alc_class = mono_object_get_class(alc);
            MonoMethod* unload = mono_class_get_method_from_name(alc_class, "Unload", 0);
            if (unload) {
                MonoObject* ex = nullptr;
                mono_runtime_invoke(unload, alc, nullptr, &ex);
            }

            MonoGCHandle weak = mono_gchandle_new_weakref_v2(alc, false);
            mono_gchandle_free_v2(static_cast<MonoGCHandle>(m_alc_gchandle));
            m_alc_gchandle = nullptr;

            MonoClass* ec = mono_class_from_name(m_core_image, "GreenCake", "ExternalCalls");
            MonoMethod* collect = ec ? mono_class_get_method_from_name(ec, "CollectAndWait", 0) : nullptr;
            for (int i = 0; i < 10; ++i) {
                if (collect) {
                    MonoObject* e = nullptr;
                    mono_runtime_invoke(collect, nullptr, nullptr, &e);
                }
                mono_gc_collect(mono_gc_max_generation());
                if (!mono_gchandle_get_target_v2(weak)) break;
            }
            if (mono_gchandle_get_target_v2(weak)) {
                DO_WARN("ScriptEngine: app ALC not reclaimed, strong references may remain");
            }
            mono_gchandle_free_v2(weak);
        } else {
            mono_gchandle_free_v2(static_cast<MonoGCHandle>(m_alc_gchandle));
            m_alc_gchandle = nullptr;
        }
    }

    Bool ScriptEngine::loadAppAssembly() {
        const auto active_project = Project::ActiveProject();
        if (!active_project) {
            return false;
        }

        mono_domain_set(m_core_domain, true);

        MonoClass* alc_class = mono_class_from_name(mono_get_corlib(), "System.Runtime.Loader", "AssemblyLoadContext");
        if (!alc_class) {
            DO_ERROR("ScriptEngine: failed to find AssemblyLoadContext class");
            return false;
        }

        MonoMethod* alc_ctor = mono_class_get_method_from_name(alc_class, ".ctor", 2);
        if (!alc_ctor) {
            DO_ERROR("ScriptEngine: failed to find AssemblyLoadContext ctor");
            return false;
        }

        MonoObject* alc_instance = mono_object_new(m_core_domain, alc_class);
        if (!alc_instance) {
            DO_ERROR("ScriptEngine: failed to allocate AssemblyLoadContext");
            return false;
        }

        MonoString* alc_name = mono_string_new(m_core_domain, "AppAssembly");
        bool is_collectible = true;
        void* ctor_args[2] = { alc_name, &is_collectible };
        MonoObject* exception = nullptr;
        mono_runtime_invoke(alc_ctor, alc_instance, ctor_args, &exception);
        if (exception) {
            DO_ERROR("ScriptEngine: failed to create AssemblyLoadContext");
            return false;
        }

        m_alc_gchandle = static_cast<void*>(mono_gchandle_new_v2(alc_instance, false));

        const fs::path assembly_path = Project::ScriptAssemblyPath();
        auto data = ReadFileBinary(assembly_path);
        if (data.empty()) {
            DO_ERROR("ScriptEngine: failed to read app assembly '{}'", assembly_path.string());
            return false;
        }

        MonoImageOpenStatus status;
        MonoImage* image = mono_image_open_from_data_alc(
            m_alc_gchandle,
            const_cast<char*>(reinterpret_cast<const char*>(data.data())),
            static_cast<uint32_t>(data.size()),
            true, &status,
            assembly_path.filename().string().c_str());

        if (status != MONO_IMAGE_OK || !image) {
            DO_ERROR("ScriptEngine: mono_image_open_from_data_alc failed, status={}", static_cast<Int>(status));
            return false;
        }

        m_app_assembly = mono_assembly_load_from_full(image, assembly_path.filename().string().c_str(), &status, false);
        mono_image_close(image);

        if (status != MONO_IMAGE_OK || !m_app_assembly) {
            DO_ERROR("ScriptEngine: mono_assembly_load_from_full failed, status={}", static_cast<Int>(status));
            return false;
        }

        m_app_image = mono_assembly_get_image(m_app_assembly);
        if (!m_app_image) {
            return false;
        }

        return true;
    }

    void ScriptEngine::cleanupMono() {
        if (m_alc_gchandle) {
            MonoObject* alc_instance = mono_gchandle_get_target_v2(static_cast<MonoGCHandle>(m_alc_gchandle));
            if (alc_instance) {
                MonoClass* alc_class = mono_object_get_class(alc_instance);
                MonoMethod* unload_method = mono_class_get_method_from_name(alc_class, "Unload", 0);
                if (unload_method) {
                    MonoObject* exception = nullptr;
                    mono_runtime_invoke(unload_method, alc_instance, nullptr, &exception);
                }
            }
            mono_gchandle_free_v2(static_cast<MonoGCHandle>(m_alc_gchandle));
            m_alc_gchandle = nullptr;
        }

        m_app_image = nullptr;
        m_app_assembly = nullptr;
        m_core_domain = nullptr;

        if (m_root_domain) {
            mono_jit_cleanup(m_root_domain);
            m_root_domain = nullptr;
        }
    }

} //dodoe
