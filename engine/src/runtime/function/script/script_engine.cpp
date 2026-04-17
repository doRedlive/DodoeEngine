// do@Redlive

#include "script_engine.h"

#include <filesystem>
#include <windows.h>

#include "mono/jit/jit.h"
#include "mono/jit/mono-private-unstable.h"
#include "mono/metadata/assembly.h"
#include "mono/metadata/appdomain.h"
#include "mono/metadata/class.h"
#include "mono/metadata/object.h"
#include "mono/metadata/threads.h"

namespace dodoe {

    namespace fs = std::filesystem;

    namespace {

        static fs::path GetExeDir() {
            std::array<char, 256> buffer{};
            DWORD len = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            return len ? fs::path(buffer.data()).parent_path() : std::filesystem::path();
        }

        static std::string ResolveTpaList(fs::path dir) {
            std::vector<std::string> tpa_list;
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".dll") {
                    tpa_list.push_back(entry.path().string());
                }
            }
            std::sort(tpa_list.begin(), tpa_list.end());

            std::string tpa;
            for (size_t i = 0; i < tpa_list.size(); ++i) {
                if (i) tpa += ';';
                tpa += tpa_list[i];
            }

            return tpa;
        }
    }


    Scope<ScriptEngine> ScriptEngine::create(const ScriptEngineCreateInfo& info) {
        if (auto engine = create_scope<ScriptEngine>(); engine->initialize(info))
            return engine;
        return nullptr;
    }

    void ScriptEngine::destroy(Scope<ScriptEngine>& engine) {
        if (!engine) return;
        engine->shutdown();
        engine.reset();
    }

    bool ScriptEngine::initialize(const ScriptEngineCreateInfo& info) {
        setupMono();
    }

    void ScriptEngine::shutdown() {
        cleanupMono();
    }

    bool ScriptEngine::setupMono() {
        const fs::path runtime_dir =
            "C:\\Users\\33235\\Redlive\\Libraries\\runtime\\artifacts\\bin\\testhost\\net8.0-windows-Release-x64\\shared\\Microsoft.NETCore.App\\8.0.25";
        const fs::path exe_dir = GetExeDir();

        const std::string tpa = ResolveTpaList(runtime_dir);
        const std::string runtime_dir_str = runtime_dir.string();
        const std::string exe_dir_str = exe_dir.string();
        const std::string app_paths = exe_dir_str + ";" + runtime_dir_str;
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

        DO_ASSERT(monovm_initialize(static_cast<int>(std::size(keys)), keys, values) == 0, "monovm initialize failed");
        mono_set_assemblies_path(runtime_dir_str.c_str());

        m_root_domain = mono_jit_init("DodoeJitRuntime");
        DO_ASSERT(m_root_domain, "MonoJit init failed");

        mono_thread_set_main(mono_thread_current());

        return true;
    }

    bool ScriptEngine::loadCoreAssembly(const std::string& path) {
        m_core_domain = mono_domain_create_appdomain(const_cast<char*>("DodoeScriptRuntime"), nullptr);
        mono_domain_set(m_core_domain, true);

        const fs::path assembly_path = GetExeDir() / "GreenCake.dll";
        m_core_assembly = mono_domain_assembly_open(m_core_domain, assembly_path.string().c_str());
        DO_ASSERT(m_core_assembly);

        m_core_image = mono_assembly_get_image(m_core_assembly);
        DO_ASSERT(m_core_image);
    }

    bool ScriptEngine::loadAppAssembly(const std::string& path) {
        // const fs::path assembly_path = GetExeDir
    }

    bool ScriptEngine::loadAssembly(const std::string& path) {

    }

    void ScriptEngine::cleanupMono() {
        mono_domain_set(mono_get_root_domain(), false);

        mono_domain_unload(m_core_domain);
        m_core_domain = nullptr;

        mono_jit_cleanup(m_root_domain);
        m_root_domain = nullptr;
    }


} //dodoe
