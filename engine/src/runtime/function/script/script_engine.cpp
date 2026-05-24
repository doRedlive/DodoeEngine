// do@Redlive

#include "script_engine.h"

#include <filesystem>
#include <windows.h>

#include "runtime/core/project/project.h"
#include "runtime/platform/platform_tool.h"

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

        bool BuildScriptSourceFingerprint(const fs::path& asset_directory, std::string& out_fingerprint) {
            out_fingerprint.clear();
            if (!fs::exists(asset_directory) || !fs::is_directory(asset_directory)) {
                return false;
            }

            std::vector<fs::path> script_files;
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
            std::array<char, 256> buffer{};
            DWORD len = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            return len ? fs::path(buffer.data()).parent_path() : std::filesystem::path();
        }

        std::string ResolveTpaList(const fs::path &dir) {
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

    bool ScriptEngine::reloadScripts() {
        const auto active_project = Project::ActiveProject();
        if (!active_project) {
            return false;
        }

        std::string script_sources_fingerprint;
        if (!BuildScriptSourceFingerprint(Project::AssetDirectory(), script_sources_fingerprint)) {
            m_script_sources_fingerprint.clear();
            return false;
        }

        if (m_app_image && script_sources_fingerprint == m_script_sources_fingerprint) {
            return false;
        }

        if (!buildScrptAssembly()) {
            return false;
        }

        unloadManagedDomain();
        if (!loadCoreAssembly("")) {
            return false;
        }
        if (!loadAppAssembly()) {
            return false;
        }

        m_script_sources_fingerprint = std::move(script_sources_fingerprint);
        return true;
    }

    bool ScriptEngine::initialize(const ScriptEngineCreateInfo& info) {
        if (!setupMono()) return false;
        if (!loadCoreAssembly("")) return false;
        return true;
    }

    void ScriptEngine::shutdown() {
        unloadManagedDomain();
        cleanupMono();
    }

    bool ScriptEngine::buildScrptAssembly() {
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

    bool ScriptEngine::setupMono() {
        const fs::path runtime_dir =
            R"(C:\Users\33235\Redlive\Libraries\runtime\artifacts\bin\testhost\net8.0-windows-Release-x64\shared\Microsoft.NETCore.App\8.0.25)";
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

    void ScriptEngine::unloadManagedDomain() {
        m_core_assembly = nullptr;
        m_app_assembly = nullptr;
        m_core_image = nullptr;
        m_app_image = nullptr;

        if (!m_core_domain) {
            return;
        }

        mono_domain_set(mono_get_root_domain(), false);
        mono_domain_unload(m_core_domain);
        m_core_domain = nullptr;
    }

    bool ScriptEngine::loadCoreAssembly(const std::string& path) {
        (void)path;
        m_core_domain = mono_domain_create_appdomain(const_cast<char*>("DodoeScriptRuntime"), nullptr);
        mono_domain_set(m_core_domain, true);

        const fs::path assembly_path = GetExeDir() / "GreenCake.dll";
        m_core_assembly = mono_domain_assembly_open(m_core_domain, assembly_path.string().c_str());
        DO_ASSERT(m_core_assembly);

        m_core_image = mono_assembly_get_image(m_core_assembly);
        DO_ASSERT(m_core_image);

        return true;
    }

    bool ScriptEngine::loadAppAssembly() {
        const auto active_project = Project::ActiveProject();
        if (!active_project) {
            return false;
        }

        m_app_assembly = mono_domain_assembly_open(m_core_domain, Project::ScriptAssemblyPath().string().c_str());
        DO_ASSERT(m_app_assembly);
        if (!m_app_assembly) {
            return false;
        }

        m_app_image = mono_assembly_get_image(m_app_assembly);
        DO_ASSERT(m_app_image);
        if (!m_app_image) {
            return false;
        }
        return true;
    }

    void ScriptEngine::cleanupMono() {
        mono_domain_set(mono_get_root_domain(), false);

        mono_domain_unload(m_core_domain);
        m_core_domain = nullptr;

        mono_jit_cleanup(m_root_domain);
        m_root_domain = nullptr;
    }


} //dodoe
