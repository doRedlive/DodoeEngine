// do@Redlive

#include "shader_library.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "shader_parameter.h"

#include <regex>

namespace dodoe {

    static String LoadShaderIncludeText(const String& include_name) {
        const auto include_path = FileSystem::GetEngineResPath() / "shaders" / include_name;
        std::ifstream include_file(include_path);
        if (!include_file.is_open()) {
            return {};
        }
        return String((std::istreambuf_iterator<char>(include_file)), std::istreambuf_iterator<char>());
    }

    static void ApplyParameterSetMacros(String& text) {
        const String defines = LoadShaderIncludeText("shader_parameter_sets.glsl");
        if (defines.empty()) {
            return;
        }
        const std::regex define_re(R"(#define\s+(DOE_\w+)\s+(\d+)\b)");
        for (std::sregex_iterator it(defines.begin(), defines.end(), define_re), end; it != end; ++it) {
            text = std::regex_replace(text, std::regex("\\b" + it->str(1) + "\\b"), it->str(2));
        }
    }

    static String ResolveShaderIncludes(String text, Int32 depth) {
        static const std::regex include_re(R"(#include\s+\"([^\"]+)\")");
        constexpr Int32 kMaxIncludeDepth = 8;
        if (depth > kMaxIncludeDepth) {
            return text;
        }
        if (text.find("#include") == String::npos) {
            return text;
        }
        if (text.find("shader_parameter_sets.glsl") != String::npos) {
            ApplyParameterSetMacros(text);
        }

        String result;
        result.reserve(text.size() + 4096);
        Size_t last_pos = 0;
        for (std::sregex_iterator it(text.begin(), text.end(), include_re), end; it != end; ++it) {
            const auto& match = *it;
            const Size_t match_pos = static_cast<Size_t>(match.position());
            result.append(text, last_pos, match_pos - last_pos);
            const String include_name(match[1].first, match[1].second);
            String include_text = LoadShaderIncludeText(include_name);
            if (include_text.empty()) {
                DO_ERROR("ShaderLibrary: failed to resolve shader include '{}'", include_name);
            } else {
                include_text = ResolveShaderIncludes(std::move(include_text), depth + 1);
                result.append(include_text);
            }
            last_pos = match_pos + static_cast<Size_t>(match.length());
        }
        result.append(text, last_pos, String::npos);
        return result;
    }

    static DynamicArray<Char> InlineShaderIncludes(const DynamicArray<Char>& source) {
        String text(source.begin(), source.end());
        text = ResolveShaderIncludes(std::move(text), 0);
        return DynamicArray<Char>(text.begin(), text.end());
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
