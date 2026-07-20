// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class ShaderLibrary;

    struct TrackedShader {
        GfxShaderHandle handle;
        String source_name;
        std::filesystem::file_time_type last_write_time;
    };

    class ShaderHotReload {
        UnorderedMap<String, TrackedShader> m_tracked;
        ShaderLibrary* m_shader_library{nullptr};
        Bool m_enabled{false};
        DynamicArray<String> m_pending_reload;

    public:
        void setEnabled(Bool enabled) { m_enabled = enabled; }
        Bool isEnabled() const { return m_enabled; }

        void setShaderLibrary(ShaderLibrary* library) { m_shader_library = library; }

        void registerShader(const String& name, const GfxShaderHandle& handle);

        void pollChanges();

        Bool hasPendingReload() const;
    };

} // namespace dodoe
