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

    class ScriptEngine {
        MonoDomain* m_root_domain{nullptr};
        MonoDomain* m_core_domain{nullptr};
        MonoAssembly* m_core_assembly{nullptr};
        MonoAssembly* m_app_assembly{nullptr};
        MonoImage* m_core_image{nullptr};
        MonoImage* m_app_image{nullptr};
    public:
        static Scope<ScriptEngine> create(const ScriptEngineCreateInfo& info);
        static void destroy(Scope<ScriptEngine>& engine);

        [[nodiscard]] MonoDomain* getCoreDomain() const { return m_core_domain; }
        [[nodiscard]] MonoImage* getCoreImage() const { return m_core_image; }
        [[nodiscard]] MonoImage* getAppImage()  const { return m_app_image;  }

    private:
        bool initialize(const ScriptEngineCreateInfo& info);
        void shutdown();

        bool setupMono();
        bool loadCoreAssembly(const std::string& path);
        bool loadAppAssembly(const std::string& path);
        void cleanupMono();
    };

} // dodoe
