// do@Redlive

#include "global_shader.h"

namespace dodoe {

    GlobalShader::GlobalShader(String name)
        : m_program{.name = std::move(name)} {}

    const String& GlobalShader::getName() const {
        return m_program.name;
    }

    const ShaderProgram& GlobalShader::getProgram() const {
        return m_program;
    }

    ShaderProgram& GlobalShader::getProgram() {
        return m_program;
    }

    void GlobalShaderMap::clear() {
        m_shaders.clear();
    }

    void GlobalShaderMap::registerStage(const String& program_name, const GfxShaderType stage, const GfxShaderHandle& shader, const ShaderReflectionData* reflection) {
        const auto it = m_shaders.try_emplace(program_name, program_name).first;
        auto& program = it->second.getProgram();

        switch (stage) {
            case GfxShaderType::Vertex:
                program.vertex_shader = shader;
                program.vertex_reflection = reflection;
                break;
            case GfxShaderType::Pixel:
                program.pixel_shader = shader;
                program.pixel_reflection = reflection;
                break;
            case GfxShaderType::Hull:
                program.hull_shader = shader;
                program.hull_reflection = reflection;
                break;
            case GfxShaderType::Domain:
                program.domain_shader = shader;
                program.domain_reflection = reflection;
                break;
            case GfxShaderType::Geometry:
                program.geometry_shader = shader;
                program.geometry_reflection = reflection;
                break;
            case GfxShaderType::Compute:
                program.compute_shader = shader;
                program.compute_reflection = reflection;
                break;
            default:
                break;
        }
    }

    const GlobalShader* GlobalShaderMap::find(const String& program_name) const {
        const auto it = m_shaders.find(program_name);
        return it != m_shaders.end() ? &it->second : nullptr;
    }

    const ShaderProgram* GlobalShaderMap::findProgram(const String& program_name) const {
        const auto* shader = find(program_name);
        return shader ? &shader->getProgram() : nullptr;
    }

} // namespace dodoe
