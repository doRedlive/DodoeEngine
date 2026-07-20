// do@Redlive

#include "native_host.h"

namespace dodoe {

    namespace {

        struct NethostGetHostFxrParams {
            size_t size;
            const HostFxrChar* assembly_path;
            const HostFxrChar* dotnet_root;
        };

        using NethostGetHostFxrPath = int(__cdecl*)(HostFxrChar* buffer, size_t* buffer_size, const NethostGetHostFxrParams* params);

        FsPath GetExeDir() {
            wchar_t buffer[MAX_PATH]{};
            const DWORD len = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(MAX_PATH));
            return len ? FsPath(buffer).parent_path() : FsPath();
        }

        String GetHostFxrFileName() {
#if defined(DO_PLATFORM_WINDOWS)
            return "hostfxr.dll";
#elif defined(DO_PLATFORM_MACOS)
            return "libhostfxr.dylib";
#else
            return "libhostfxr.so";
#endif
        }

        // Decode an HRESULT into facility + code for diagnostics
        struct HResultDecoded {
            uint32_t hr;
            uint32_t severity;   // 0=success, 1=info, 2=warning, 3=error
            uint32_t facility;
            uint32_t code;
        };
        HResultDecoded DecodeHRESULT(int32_t rc) {
            const uint32_t hr = static_cast<uint32_t>(rc);
            return {
                hr,
                (hr >> 30) & 0x3,
                (hr >> 16) & 0x1FFF,
                hr & 0xFFFF
            };
        }

        // Known HRESULT facilities for .NET hosting
        const char* FacilityName(uint32_t facility) {
            switch (facility) {
                case 0x00: return "FACILITY_NULL";
                case 0x01: return "FACILITY_RPC";
                case 0x02: return "FACILITY_DISPATCH";
                case 0x03: return "FACILITY_STORAGE";
                case 0x04: return "FACILITY_ITF";
                case 0x07: return "FACILITY_WIN32";
                case 0x08: return "FACILITY_WINDOWS";
                case 0x09: return "FACILITY_SSPI";
                case 0x0A: return "FACILITY_SECURITY";
                case 0x0B: return "FACILITY_CONTROL";
                case 0x0C: return "FACILITY_CERT";
                case 0x0D: return "FACILITY_INTERNET";
                case 0x0E: return "FACILITY_MEDIASERVER";
                case 0x0F: return "FACILITY_MSMQ";
                case 0x10: return "FACILITY_TAPI";
                case 0x11: return "FACILITY_SCARD";
                case 0x12: return "FACILITY_COMPLUS";
                case 0x13: return "FACILITY_URT(.NET Runtime)";
                case 0x17: return "FACILITY_URT2";
                default:   return "UNKNOWN";
            }
        }

        // Get a system message for an HRESULT, returns empty string if not found
        String GetHRESULTMessage(int32_t rc) {
            const uint32_t hr = static_cast<uint32_t>(rc);
            wchar_t* sys_msg = nullptr;
            const DWORD msg_len = FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPWSTR>(&sys_msg), 0, nullptr);
            if (sys_msg && msg_len) {
                // trim trailing whitespace/newline
                DWORD len = msg_len;
                while (len > 0 && (sys_msg[len - 1] == L'\n' || sys_msg[len - 1] == L'\r' || sys_msg[len - 1] == L' '))
                    sys_msg[--len] = L'\0';
                String result(sys_msg, sys_msg + len);
                LocalFree(sys_msg);
                return result;
            }
            return {};
        }

        // Log a detailed HRESULT diagnostic
        void LogHRESULT(const char* context, int32_t rc) {
            const auto d = DecodeHRESULT(rc);
            const String sys_msg = GetHRESULTMessage(rc);
            if (!sys_msg.empty()) {
                DO_ERROR("NativeHost: {} hr=0x{:08X} severity={} facility=0x{:X}({}) code=0x{:X}({}) sysmsg=\"{}\"",
                    context, d.hr, d.severity, d.facility, FacilityName(d.facility), d.code, d.code, sys_msg);
            } else {
                DO_ERROR("NativeHost: {} hr=0x{:08X} severity={} facility=0x{:X}({}) code=0x{:X}({}) (no system message)",
                    context, d.hr, d.severity, d.facility, FacilityName(d.facility), d.code, d.code);
            }
        }

    } // namespace

    Bool NativeHost::initialize(const NativeHostCreateInfo& info) {
        if (loadHostFxr() != HostError::Ok) {
            DO_ERROR("NativeHost: loadHostFxr failed");
            return false;
        }
        if (initializeRuntime() != HostError::Ok) {
            DO_ERROR("NativeHost: initializeRuntime failed");
            return false;
        }
        if (loadAssembly() != HostError::Ok) {
            DO_ERROR("NativeHost: loadAssembly failed");
            return false;
        }
        return true;
    }

    void NativeHost::shutdown() {
        m_load_assembly_fn = nullptr;

        if (m_close_fn && m_context) {
            m_close_fn(m_context);
            m_context = nullptr;
        }

        m_init_fn = nullptr;
        m_get_delegate_fn = nullptr;
        m_close_fn = nullptr;

        if (m_hostfxr_dll) {
            FreeLibrary(m_hostfxr_dll);
            m_hostfxr_dll = nullptr;
        }
    }

    NativeHost::HostError NativeHost::loadHostFxr() {
        const FsPath exe_dir = GetExeDir();
        const FsPath nethost_path = exe_dir / L"nethost.dll";

        const HMODULE nethost = LoadLibraryW(nethost_path.c_str());
        if (!nethost) {
            DO_ERROR("NativeHost: nethost.dll not found at '{}' (GetLastError={})",
                nethost_path.string(), GetLastError());
            return HostError::HostFxrNotFound;
        }

        const auto get_hostfxr_path_fn = reinterpret_cast<NethostGetHostFxrPath>(
            GetProcAddress(nethost, "get_hostfxr_path"));
        if (!get_hostfxr_path_fn) {
            DO_ERROR("NativeHost: get_hostfxr_path not found in nethost.dll");
            FreeLibrary(nethost);
            return HostError::HostFxrNotFound;
        }

        HostFxrChar hostfxr_path[MAX_PATH]{};
        size_t path_size = MAX_PATH;
        const int rc = get_hostfxr_path_fn(hostfxr_path, &path_size, nullptr);
        FreeLibrary(nethost);

        if (rc != 0) {
            LogHRESULT("get_hostfxr_path failed", rc);
            return HostError::HostFxrNotFound;
        }

        m_hostfxr_dll = LoadLibraryW(hostfxr_path);
        if (!m_hostfxr_dll) {
            DO_ERROR("NativeHost: failed to load hostfxr from '{}' (GetLastError={})",
                String(hostfxr_path, hostfxr_path + wcslen(hostfxr_path)), GetLastError());
            return HostError::HostFxrLoadFailed;
        }

        return HostError::Ok;
    }

    NativeHost::HostError NativeHost::initializeRuntime() {
        if (!m_hostfxr_dll) {
            return HostError::HostFxrNotFound;
        }

        m_init_fn = reinterpret_cast<HostFxrInitConfig>(
            GetProcAddress(m_hostfxr_dll, "hostfxr_initialize_for_runtime_config"));
        m_get_delegate_fn = reinterpret_cast<HostFxrGetDelegate>(
            GetProcAddress(m_hostfxr_dll, "hostfxr_get_runtime_delegate"));
        m_close_fn = reinterpret_cast<HostFxrClose>(
            GetProcAddress(m_hostfxr_dll, "hostfxr_close"));

        if (!m_init_fn || !m_get_delegate_fn) {
            DO_ERROR("NativeHost: hostfxr exports missing (init={} get_delegate={})",
                reinterpret_cast<void*>(m_init_fn), reinterpret_cast<void*>(m_get_delegate_fn));
            return HostError::HostFxrLoadFailed;
        }

        // Set up hostfxr error writer for detailed diagnostics
        const auto set_error_writer_fn = reinterpret_cast<HostFxrSetErrorWriter>(
            GetProcAddress(m_hostfxr_dll, "hostfxr_set_error_writer"));
        if (set_error_writer_fn) {
            const auto prev_writer = set_error_writer_fn([](const HostFxrChar* msg) {
                DO_ERROR("NativeHost: [hostfxr] {}", String(msg, msg + wcslen(msg)));
            });
            (void)prev_writer;
        }

        const FsPath runtimeconfig = GetExeDir() / L"GreenCake.runtimeconfig.json";

        if (!std::filesystem::exists(runtimeconfig)) {
            DO_ERROR("NativeHost: runtime config NOT FOUND at '{}'", runtimeconfig.string());
            return HostError::RuntimeInitFailed;
        }

        const std::wstring exe_dir_wide = GetExeDir().wstring();

        HostFxrInitParams init_params{};
        init_params.size = sizeof(HostFxrInitParams);
        init_params.host_path = exe_dir_wide.c_str();
        init_params.dotnet_root = nullptr;

        const std::wstring config_wide = runtimeconfig.wstring();
        const int init_rc = m_init_fn(config_wide.c_str(), &init_params, &m_context);

        if (init_rc != 0 || !m_context) {
            LogHRESULT("runtime init failed", init_rc);
            return HostError::RuntimeInitFailed;
        }

        // Set APP_CONTEXT_BASE_DIRECTORY so CoreCLR can resolve assembly paths
        m_set_prop_fn = reinterpret_cast<HostFxrSetProp>(
            GetProcAddress(m_hostfxr_dll, "hostfxr_set_runtime_property_value"));
        if (m_set_prop_fn) {
            const std::wstring base_dir = GetExeDir().wstring();
            m_set_prop_fn(m_context, L"APP_CONTEXT_BASE_DIRECTORY", base_dir.c_str());
            m_set_prop_fn(m_context, L"PROBING_DIRECTORIES", base_dir.c_str());
        }
        return HostError::Ok;
    }

    NativeHost::HostError NativeHost::loadAssembly() {
        const int rc = m_get_delegate_fn(m_context, HDT_LOAD_ASM_AND_GET_FN,
            reinterpret_cast<void**>(&m_load_assembly_fn));
        if (rc != 0) {
            LogHRESULT("get_runtime_delegate(LOAD_ASM_AND_GET_FN) failed", rc);
            return HostError::DelegateLoadFailed;
        }
        if (!m_load_assembly_fn) {
            DO_ERROR("NativeHost: load_assembly_fn is null after successful call");
            return HostError::DelegateLoadFailed;
        }
        return HostError::Ok;
    }

    void* NativeHost::loadManagedDelegate(
        const std::wstring& assembly_path,
        const std::wstring& type_name,
        const std::wstring& method_name,
        const wchar_t* delegate_type) {
        if (!m_load_assembly_fn) {
            DO_ERROR("NativeHost: loadManagedDelegate called with null m_load_assembly_fn");
            return nullptr;
        }

        FsPath asm_path(assembly_path);
        if (asm_path.is_relative()) {
            wchar_t exe_dir[MAX_PATH]{};
            const DWORD len = GetModuleFileNameW(nullptr, exe_dir, static_cast<DWORD>(MAX_PATH));
            if (len) {
                asm_path = FsPath(exe_dir).parent_path() / asm_path;
            }
        }
        if (!std::filesystem::exists(asm_path)) {
            DO_ERROR("NativeHost: assembly file NOT FOUND at '{}'", asm_path.string());
            return nullptr;
        }

        void* delegate = nullptr;
        const std::wstring resolved_path = asm_path.wstring();
        const int rc = m_load_assembly_fn(
            resolved_path.c_str(),
            type_name.c_str(),
            method_name.c_str(),
            delegate_type,
            nullptr,
            &delegate);

        if (rc != 0 || !delegate) {
            if (rc != 0) {
                LogHRESULT("loadManagedDelegate", rc);
            } else {
                DO_ERROR("NativeHost: loadManagedDelegate returned null delegate (rc=0)");
            }
            return nullptr;
        }
        return delegate;
    }

} // namespace dodoe
