// do@Redlive

#include "shader_hot_reload.h"

#include "shader_manifest.h"
#include "shader_library.h"
#include "runtime/resource/file/file_system.h"

namespace dodoe {

    void ShaderHotReload::registerShader(const String& name, const GfxShaderHandle& handle) {
        if (!m_enabled) {
            return;
        }

        const auto* manifest_entry = m_shader_library ? m_shader_library->getManifest().find(name) : nullptr;
        if (!manifest_entry) {
            return;
        }

        TrackedShader tracked;
        tracked.handle = handle;
        tracked.source_name = manifest_entry->source;

        auto source_path = FileSystem::GetEngineResPath() / "shaders" / (manifest_entry->source + ".hlsl");
        if (std::filesystem::exists(source_path)) {
            tracked.last_write_time = std::filesystem::last_write_time(source_path);
        }

        m_tracked[name] = tracked;
    }

    void ShaderHotReload::pollChanges() {
        if (!m_enabled) {
            return;
        }

        m_pending_reload.clear();

        for (const auto& [name, tracked] : m_tracked) {
            auto source_path = FileSystem::GetEngineResPath() / "shaders" / (tracked.source_name + ".hlsl");

            if (!std::filesystem::exists(source_path)) {
                continue;
            }

            auto current_time = std::filesystem::last_write_time(source_path);
            if (current_time != tracked.last_write_time) {
                m_pending_reload.push_back(name);
                m_tracked[name].last_write_time = current_time;
            }
        }

        for (const auto& name : m_pending_reload) {
            DO_INFO("ShaderHotReload detected change: {}", name);
        }
    }

    Bool ShaderHotReload::hasPendingReload() const {
        return !m_pending_reload.empty();
    }

} // namespace dodoe
