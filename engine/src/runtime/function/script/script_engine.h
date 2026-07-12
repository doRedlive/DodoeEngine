#pragma once

#include "dopch.h"
#include "native_host.h"

namespace dodoe {

    using ScriptCallFn = int (*)(const char* method, void** args, void** result);

    struct ScriptEngineCreateInfo {

    };

    class ScriptEngine : public Managed<ScriptEngine, ScriptEngineCreateInfo> {
        friend class Managed<ScriptEngine, ScriptEngineCreateInfo>;
        Scope<NativeHost> m_native_host;
        ScriptCallFn m_call{nullptr};
        void* m_alc_gchandle{nullptr};
        std::string m_script_sources_fingerprint{};

    public:
        [[nodiscard]] ScriptCallFn getCallFn() const { return m_call; }
        [[nodiscard]] void* getAlcHandle() const { return m_alc_gchandle; }

        bool onScriptSourcesChanged();
        bool buildAppAssembly();
        void unloadAppAssembly();
        bool loadAppAssembly();
        void commitScriptFingerprint();

    private:
        bool initialize(const ScriptEngineCreateInfo& info);
        void shutdown();

        bool loadCoreAssembly();

        std::string m_pending_fingerprint{};
        int m_reload_counter{0};
    };

} // dodoe
