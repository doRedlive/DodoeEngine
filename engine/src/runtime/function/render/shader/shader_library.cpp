// do@Redlive

#include "shader_library.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "shader_parameter.h"

#include <regex>

namespace dodoe {

    static DynamicArray<Char> InlineShaderIncludes(const DynamicArray<Char>& source) {
        const String marker = "#include \"shader_parameter_sets.glsl\"";
        String text(source.begin(), source.end());
        const Size_t marker_pos = text.find(marker);
        if (marker_pos == String::npos) {
            return source;
        }

        const auto include_path = FileSystem::GetEngineResPath() / "shaders" / "shader_parameter_sets.glsl";
        std::ifstream include_file(include_path);
        if (include_file.is_open()) {
            const String defines((std::istreambuf_iterator<char>(include_file)), std::istreambuf_iterator<char>());
            const std::regex define_re(R"(#define\s+(DOE_\w+)\s+(\d+)\b)");
            for (std::sregex_iterator it(defines.begin(), defines.end(), define_re), end; it != end; ++it) {
                text = std::regex_replace(text, std::regex("\\b" + it->str(1) + "\\b"), it->str(2));
            }
        }

        String result;
        result.reserve(text.size() + 2048);
        result.append(text, 0, marker_pos);
        result.append(text, marker_pos + marker.size(), String::npos);
        return DynamicArray<Char>(result.begin(), result.end());
    }

    Bool ShaderLibrary::initialize(const ShaderLibraryCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("ShaderLibrary::initialize", "startup");
        if (!info.gfx_context) {
            DO_ERROR("ShaderLibrary::initialize: graphics context is unavailable");
            return false;
        }

        if (!m_manifest.loadFromFile("shaders/shader_manifest.json")) {
            DO_ERROR("ShaderLibrary::initialize failed to load shader manifest");
            return false;
        }

        const auto api = RenderSettings::GetRenderBackendApiType();
        const char* backend_ext = (api == RenderBackendApiType::D3D12) ? ".dxil" : ".spv";
        DO_INFO("ShaderLibrary: loading shaders for backend {} (bindless={})",
            RenderSettings::GetRenderBackendApiTypeStr(), RenderSettings::IsBindlessActive());

        const char* platform_str = nullptr;
        switch (api) {
            case RenderBackendApiType::D3D12:   platform_str = "d3d12";   break;
            case RenderBackendApiType::Vulkan: platform_str = "vulkan"; break;
            case RenderBackendApiType::OpenGL: platform_str = "opengl"; break;
            default: platform_str = ""; break;
        }

        for (const auto& entry : m_manifest.getEntries()) {
            if (!entry.platforms.empty()) {
                Bool supported = false;
                for (const auto& p : entry.platforms) {
                    if (p == platform_str) {
                        supported = true;
                        break;
                    }
                }
                if (!supported) continue;
            }

            if (!RenderSettings::IsBindlessActive() &&
                (entry.name == "GBufferPS" || entry.name == "SpritePS" || entry.name == "UIPS" ||
                 entry.name == "ForwardLitPS")) {
                continue;
            }

            const Bool use_glsl_source = api == RenderBackendApiType::OpenGL;
            String file_name = entry.source + ShaderManifest::StageToExtension(entry.stage);
            String path = use_glsl_source ? "shaders/" + file_name : "shaders/bin/" + file_name + backend_ext;

            auto source = ReadShaderFile(path);
            if (source.empty()) {
                DO_ERROR("ShaderLibrary::initialize failed to read shader file: {}", path);
                continue;
            }
            if (use_glsl_source) {
                source = InlineShaderIncludes(source);
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

            const String reflection_path = "shaders/bin/" + file_name + ".spv";
            const auto reflection_bytecode = ReadShaderFile(reflection_path);
            if (reflection_bytecode.empty()) {
                auto refl = ShaderReflector::Reflect(shader, entry.name);
                if (refl.valid()) {
                    m_reflections[entry.name] = std::move(refl);
                }
            } else {
                const DynamicArray<UInt8> reflection_bytes(
                    reflection_bytecode.begin(), reflection_bytecode.end());
                auto refl = ShaderReflector::ReflectBytecode(reflection_bytes, entry.stage, entry.name);
                if (refl.valid()) {
                    m_reflections[entry.name] = std::move(refl);
                }
            }
        }

        DO_INFO("ShaderLibrary::initialize loaded {} shaders, {} reflections",
                m_shaders.size(), m_reflections.size());
        return !m_shaders.empty();
    }

    void ShaderLibrary::shutdown() { reset(); }

    void ShaderLibrary::reset() {
        DO_PROFILE_SCOPE_CATEGORY("ShaderLibrary::reset", "shutdown");
        DO_INFO("ShaderLibrary: releasing {} shader(s) and {} reflection(s)",
            m_shaders.size(), m_reflections.size());
        ClearStaticBindingLayoutCaches();
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
