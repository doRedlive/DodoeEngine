// do@Redlive

#include "shader_library.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    void ShaderLibrary::initialize(GfxContext& gfx_context) {
        (void)gfx_context;

        if (!m_manifest.loadFromFile("shaders/shader_manifest.json")) {
            DO_ERROR("ShaderLibrary::initialize failed to load shader manifest");
            return;
        }

        const auto api = RenderSettings::GetRenderBackendApiType();
        const char* backend_ext = (api == RenderBackendApiType::DX12) ? ".dxil" : ".spv";

        for (const auto& entry : m_manifest.getEntries()) {
            String file_name = entry.source + ShaderManifest::StageToExtension(entry.stage) + backend_ext;
            String path = "shaders/bin/" + file_name;

            auto source = ReadShaderFile(path);
            if (source.empty()) {
                DO_ERROR("ShaderLibrary::initialize failed to read shader file: {}", path);
                continue;
            }

            String debug_name = "ShaderLibrary " + entry.name;
            auto shader = GDrawCommandList.createShader(
                GfxShaderDesc().setShaderType(entry.stage).setEntryName(entry.entry_point.c_str()).setDebugName(debug_name.c_str()),
                source.data(),
                source.size()
            );
            if (!shader) {
                DO_ERROR("ShaderLibrary::initialize createShader failed for: {}", path);
                continue;
            }

            m_shaders[entry.name] = shader;

            auto refl = ShaderReflector::Reflect(shader, entry.name);
            if (refl.valid()) {
                m_reflections[entry.name] = std::move(refl);
            }
        }

        DO_INFO("ShaderLibrary::initialize loaded {} shaders, {} reflections",
                m_shaders.size(), m_reflections.size());
    }

    void ShaderLibrary::reset() {
        m_shaders.clear();
        m_reflections.clear();
    }

    const GfxShaderHandle* ShaderLibrary::findShader(const String& name) const {
        auto it = m_shaders.find(name);
        if (it != m_shaders.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const ShaderReflectionData* ShaderLibrary::getReflection(const String& name) const {
        auto it = m_reflections.find(name);
        if (it != m_reflections.end()) {
            return &it->second;
        }
        return nullptr;
    }

} // namespace dodoe
