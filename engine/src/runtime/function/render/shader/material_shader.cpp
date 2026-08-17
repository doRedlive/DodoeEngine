// do@Redlive

#include "material_shader.h"

namespace dodoe {

    MaterialShader::MaterialShader(String name)
        : m_name(std::move(name)) {}

    const String& MaterialShader::getName() const {
        return m_name;
    }

    ShaderProgram& MaterialShader::getOrCreateVariant(const String& variant_name) {
        auto [it, inserted] = m_variants.try_emplace(variant_name);
        if (inserted) {
            it->second.name = m_name + ":" + variant_name;
        }
        return it->second;
    }

    const ShaderProgram* MaterialShader::findVariant(const String& variant_name) const {
        const auto it = m_variants.find(variant_name);
        return it != m_variants.end() ? &it->second : nullptr;
    }

    void MaterialShader::finalizeVariants() {
        const auto* default_variant = findVariant("default");
        if (!default_variant) {
            return;
        }

        for (auto& [name, variant] : m_variants) {
            if (!variant.vertex_shader) {
                variant.vertex_shader = default_variant->vertex_shader;
                variant.vertex_reflection = default_variant->vertex_reflection;
            }
            if (!variant.hull_shader) {
                variant.hull_shader = default_variant->hull_shader;
                variant.hull_reflection = default_variant->hull_reflection;
            }
            if (!variant.domain_shader) {
                variant.domain_shader = default_variant->domain_shader;
                variant.domain_reflection = default_variant->domain_reflection;
            }
            if (!variant.geometry_shader) {
                variant.geometry_shader = default_variant->geometry_shader;
                variant.geometry_reflection = default_variant->geometry_reflection;
            }
            if (!variant.pixel_shader) {
                variant.pixel_shader = default_variant->pixel_shader;
                variant.pixel_reflection = default_variant->pixel_reflection;
            }
            if (!variant.compute_shader) {
                variant.compute_shader = default_variant->compute_shader;
                variant.compute_reflection = default_variant->compute_reflection;
            }
        }
    }

    void MaterialShaderMap::clear() {
        m_shaders.clear();
    }

    void MaterialShaderMap::registerStage(const String& program_name, const String& variant_name, const GfxShaderType stage, const GfxShaderHandle& shader, const ShaderReflectionData* reflection) {
        const auto it = m_shaders.try_emplace(program_name, program_name).first;
        auto& program = it->second.getOrCreateVariant(variant_name);

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

    void MaterialShaderMap::finalize() {
        for (auto& [name, shader] : m_shaders) {
            shader.finalizeVariants();
        }
    }

    const MaterialShader* MaterialShaderMap::find(const String& program_name) const {
        const auto it = m_shaders.find(program_name);
        return it != m_shaders.end() ? &it->second : nullptr;
    }

    const ShaderProgram* MaterialShaderMap::findProgram(const String& program_name, const String& variant_name) const {
        const auto* shader = find(program_name);
        if (!shader) {
            return nullptr;
        }
        const auto* variant = shader->findVariant(variant_name);
        return variant ? variant : shader->findVariant("default");
    }

} // namespace dodoe
