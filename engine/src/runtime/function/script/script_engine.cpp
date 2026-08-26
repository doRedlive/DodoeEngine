#include "dopch.h"

#include <windows.h>

#include "script_engine.h"

#include "runtime/core/project/project.h"
#include "runtime/platform/platform_tool.h"

namespace dodoe {

    namespace fs = std::filesystem;

    namespace {

        struct PeMetaStream {
            ui32 offset;
            ui32 size;
            char name[12];
        };

        struct IMAGE_COR20_HEADER_DATA {
            ui32 cb;
            ui16 MajorRuntimeVersion;
            ui16 MinorRuntimeVersion;
            IMAGE_DATA_DIRECTORY MetaData;
            ui32 Flags;
            ui32 EntryPointToken;
            IMAGE_DATA_DIRECTORY Resources;
            IMAGE_DATA_DIRECTORY StrongNameSignature;
            IMAGE_DATA_DIRECTORY CodeManagerTable;
            IMAGE_DATA_DIRECTORY VTableFixups;
            IMAGE_DATA_DIRECTORY ExportAddressTableJumps;
            IMAGE_DATA_DIRECTORY ManagedNativeHeader;
        };

        static ui32 PeRvaToOffset(const Byte* base, ui32 rva) {
            auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            auto* nth = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
            auto* sec = IMAGE_FIRST_SECTION(nth);
            for (int i = 0; i < nth->FileHeader.NumberOfSections; ++i, ++sec) {
                if (rva >= sec->VirtualAddress && rva < sec->VirtualAddress + sec->Misc.VirtualSize)
                    return sec->PointerToRawData + (rva - sec->VirtualAddress);
            }
            return 0;
        }

        Bool BuildScriptSourceFingerprint(const FsPath& asset_directory, String& out_fingerprint) {
            out_fingerprint.clear();
            if (!fs::exists(asset_directory) || !fs::is_directory(asset_directory)) {
                return false;
            }

            DynamicArray<FsPath> script_files;
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

        DynamicArray<Byte> ReadFileBinary(const FsPath& filepath) {
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

        if (m_call && script_sources_fingerprint == m_script_sources_fingerprint) {
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
        DO_PROFILE_SCOPE_CATEGORY("ScriptEngine::initialize", "startup");
        m_native_host = NativeHost::Create({});
        if (!m_native_host) {
            DO_ERROR("ScriptEngine: NativeHost creation failed");
            return false;
        }

        if (!loadCoreAssembly()) return false;
        if (!loadAppAssembly()) return false;
        return true;
    }

    void ScriptEngine::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("ScriptEngine::shutdown", "shutdown");
        unloadAppAssembly(false);

        if (m_native_host) {
            NativeHost::Destroy(m_native_host);
        }
    }

    Bool ScriptEngine::buildAppAssembly() {
        DO_PROFILE_SCOPE_CATEGORY("ScriptEngine::buildAppAssembly", "script");
        const auto active_project = Project::ActiveProject();
        if (PlatformTool::BuildCSharpAssembly(Project::AssetDirectory(), Project::BinariesDirectory(), active_project->config().name)) {
            return true;
        }
        DO_ERROR("ScriptEngine build script assembly failed!");
        return false;
    }

    Bool ScriptEngine::loadCoreAssembly() {
        m_call = (ScriptCallFn)m_native_host->loadManagedDelegate(
            L"GreenCake.dll",
            L"GreenCake.ScriptHub, GreenCake",
            L"Call",
            reinterpret_cast<const wchar_t*>(-1));

        if (!m_call) {
            DO_ERROR("ScriptEngine: failed to load ScriptHub_Call delegate");
            return false;
        }

        return true;
    }

    void ScriptEngine::unloadAppAssembly(Bool collect_garbage) {
        if (!m_alc_gchandle) {
            return;
        }

        if (m_call) {
            void* alc_arg = m_alc_gchandle;
            m_call("unload_app", &alc_arg, nullptr);
            if (collect_garbage) {
                m_call("gc_collect", nullptr, nullptr);
            }
        }

        m_alc_gchandle = nullptr;
    }

    Bool ScriptEngine::loadAppAssembly() {
        DO_PROFILE_SCOPE_CATEGORY("ScriptEngine::loadAppAssembly", "startup");
        const auto active_project = Project::ActiveProject();
        if (!active_project) {
            DO_ERROR("ScriptEngine: loadAppAssembly failed, no active project");
            return false;
        }

        if (!m_call) {
            DO_ERROR("ScriptEngine: ScriptHub_Call not loaded");
            return false;
        }

        const FsPath assembly_path = fs::absolute(Project::ScriptAssemblyPath());
        auto data = ReadFileBinary(assembly_path);
        if (data.empty()) {
            DO_ERROR("ScriptEngine: failed to read app assembly '{}'", assembly_path.string());
            return false;
        }
        DO_PROFILE_MARK("ScriptEngine::loadAppAssembly.readAssembly", "startup");

        ++m_reload_counter;

        String assembly_path_str;
#ifdef DO_PLATFORM_WINDOWS
        const std::wstring wpath = assembly_path.wstring();
        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), (int)wpath.length(), nullptr, 0, nullptr, nullptr);
        if (utf8_len > 0) {
            assembly_path_str.resize(utf8_len);
            WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), (int)wpath.length(), &assembly_path_str[0], utf8_len, nullptr, nullptr);
        }
#else
        assembly_path_str = assembly_path.string();
#endif
        
        void* result = nullptr;
        void* args[3] = {
            (void*)data.data(),
            (void*)(intptr_t)static_cast<int>(data.size()),
            (void*)assembly_path_str.c_str()
        };

        DO_PROFILE_MARK("ScriptEngine::loadAppAssembly.invokeManaged", "startup");
        int rc = m_call("load_app_assembly", args, &result);
        if (rc != 1 || !result) {
            DO_ERROR("ScriptEngine: load_app_assembly failed");
            return false;
        }

        m_alc_gchandle = result;
        return true;
    }

} //dodoe
