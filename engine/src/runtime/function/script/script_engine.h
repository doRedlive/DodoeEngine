#pragma once

#include "dopch.h"
#include "native_host.h"
#include "script_command.h"

namespace dodoe {

    struct ScriptEngineCreateInfo {

    };

    class ScriptEngine : public Managed<ScriptEngine, ScriptEngineCreateInfo> {
        friend class Managed<ScriptEngine, ScriptEngineCreateInfo>;
        Scope<NativeHost> m_native_host;
        ScriptCallFn m_call{nullptr};
        ScriptLifecycleFn m_invoke_start{nullptr};
        ScriptLifecycleFn m_invoke_update{nullptr};
        ScriptLifecycleFn m_invoke_fixed_update{nullptr};
        ScriptLifecycleFn m_invoke_finalize{nullptr};
        void* m_alc_gchandle{nullptr};
        String m_script_sources_fingerprint{};

    public:
        [[nodiscard]] ScriptCallFn getCallFn() const { return m_call; }
        [[nodiscard]] ScriptLifecycleFn getInvokeStartFn() const { return m_invoke_start; }
        [[nodiscard]] ScriptLifecycleFn getInvokeUpdateFn() const { return m_invoke_update; }
        [[nodiscard]] ScriptLifecycleFn getInvokeFixedUpdateFn() const { return m_invoke_fixed_update; }
        [[nodiscard]] ScriptLifecycleFn getInvokeFinalizeFn() const { return m_invoke_finalize; }
        [[nodiscard]] void* getAlcHandle() const { return m_alc_gchandle; }

        bool onScriptSourcesChanged();
        bool buildAppAssembly();
        bool autoBuildAppAssembly();
        void unloadAppAssembly(Bool collect_garbage = true);
        bool loadAppAssembly();
        void commitScriptFingerprint();

    private:
        bool initialize(const ScriptEngineCreateInfo& info);
        void shutdown();

        bool loadCoreAssembly();

        String m_pending_fingerprint{};
        int m_reload_counter{0};
    };

} // dodoe
