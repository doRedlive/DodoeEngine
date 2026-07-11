// do@GreenMuffin

#pragma once

#include "dopch.h"

extern "C" {
    typedef struct _MonoDomain MonoDomain;
    typedef struct _MonoAssembly MonoAssembly;
    typedef struct _MonoImage MonoImage;
}

namespace dodoe {

    struct ScriptEngineCreateInfo {

    };

    class ScriptEngine : public Managed<ScriptEngine, ScriptEngineCreateInfo> {
        friend class Managed<ScriptEngine, ScriptEngineCreateInfo>;
        MonoDomain* m_root_domain{nullptr};
        MonoDomain* m_core_domain{nullptr};
        MonoAssembly* m_core_assembly{nullptr};
        MonoAssembly* m_app_assembly{nullptr};
        MonoImage* m_core_image{nullptr};
        MonoImage* m_app_image{nullptr};
        void* m_alc_gchandle{nullptr};
        std::string m_script_sources_fingerprint{};

    public:
        [[nodiscard]] MonoDomain* getCoreDomain() const { return m_core_domain; }
        [[nodiscard]] MonoImage* getCoreImage() const { return m_core_image; }
        [[nodiscard]] MonoImage* getAppImage()  const { return m_app_image;  }

        bool onScriptSourcesChanged();
        bool buildAppAssembly();
        void resetManagedState();
        void unloadAppAssembly();
        bool loadAppAssembly();
        void commitScriptFingerprint();

    private:
        bool initialize(const ScriptEngineCreateInfo& info);
        void shutdown();

        bool setupMono();
        void cleanupMono();

        bool loadCoreAssembly(const String& path);

        std::string m_pending_fingerprint{};
        int m_reload_counter{0};
    };

} // dodoe
