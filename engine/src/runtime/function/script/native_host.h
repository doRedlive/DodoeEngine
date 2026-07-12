// do@Redlive

#pragma once

#include "dopch.h"

#include <windows.h>

#ifdef _WIN32
    using HostFxrChar = wchar_t;
#else
    using HostFxrChar = char;
#endif

using HostFxrHandle      = void*;
using HostFxrInitConfig  = int32_t(__cdecl*)(const HostFxrChar*, const struct HostFxrInitParams*, HostFxrHandle*);
using HostFxrGetDelegate = int32_t(__cdecl*)(HostFxrHandle, int32_t, void**);
using HostFxrClose       = int32_t(__cdecl*)(HostFxrHandle);
using HostFxrErrorWriter = void(__cdecl*)(const HostFxrChar*);
using HostFxrSetErrorWriter = HostFxrErrorWriter(__cdecl*)(HostFxrErrorWriter);
using HostFxrSetProp     = int32_t(__cdecl*)(HostFxrHandle, const HostFxrChar*, const HostFxrChar*);
using LoadAsmAndGetFn    = int32_t(__stdcall*)(const HostFxrChar*, const HostFxrChar*, const HostFxrChar*, const HostFxrChar*, void*, void**);

struct HostFxrInitParams {
    size_t size;
    const HostFxrChar* host_path;
    const HostFxrChar* dotnet_root;
};

enum HostFxrDelegateType : int32_t {
    HDT_COM_ACTIVATION = 0,
    HDT_LOAD_IN_MEMORY_ASSEMBLY = 1,
    HDT_WINRT_ACTIVATION = 2,
    HDT_COM_REGISTER = 3,
    HDT_COM_UNREGISTER = 4,
    HDT_LOAD_ASM_AND_GET_FN = 5,
    HDT_GET_FUNCTION_POINTER = 6,
};

namespace dodoe {

    struct NativeHostCreateInfo {
        String dotnet_root;
    };

    class NativeHost : public Managed<NativeHost, NativeHostCreateInfo> {
        friend class Managed<NativeHost, NativeHostCreateInfo>;
        enum class HostError {
            Ok = 0,
            HostFxrNotFound,
            HostFxrLoadFailed,
            RuntimeInitFailed,
            DelegateLoadFailed,
            AssemblyLoadFailed,
            FunctionNotFound,
            Unknown,
        };

    public:
        [[nodiscard]] HostFxrHandle getContext() const { return m_context; }
        [[nodiscard]] LoadAsmAndGetFn getLoadAssemblyFn() const { return m_load_assembly_fn; }

        void* loadManagedDelegate(
            const std::wstring& assembly_path,
            const std::wstring& type_name,
            const std::wstring& method_name,
            const wchar_t* delegate_type
        );

    private:
        HostError loadHostFxr();
        HostError initializeRuntime();
        HostError loadAssembly();

        Bool initialize(const NativeHostCreateInfo& info);
        void shutdown();

        HMODULE m_hostfxr_dll = nullptr;
        HostFxrHandle m_context = nullptr;
        HostFxrInitConfig m_init_fn = nullptr;
        HostFxrGetDelegate m_get_delegate_fn = nullptr;
        HostFxrClose m_close_fn = nullptr;
        HostFxrSetProp m_set_prop_fn = nullptr;
        LoadAsmAndGetFn m_load_assembly_fn = nullptr;
    };

} // namespace dodoe