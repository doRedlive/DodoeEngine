// do@Redlive

#pragma once

#include "runtime/function/render/shader/shader_program.h"

namespace dodoe {

    class MaterialShader {
    public:
        explicit MaterialShader(String name = {});

        [[nodiscard]] const String& getName() const;
        ShaderProgram& getOrCreateVariant(const String& variant_name);
        [[nodiscard]] const ShaderProgram* findVariant(const String& variant_name) const;
        void finalizeVariants();

    private:
        String m_name{};
        UnorderedMap<String, ShaderProgram> m_variants{};
    };

    class MaterialShaderMap {
    public:
        void clear();
        void registerStage(const String& program_name, const String& variant_name, GfxShaderType stage, const GfxShaderHandle& shader, const ShaderReflectionData* reflection);
        void finalize();

        [[nodiscard]] const MaterialShader* find(const String& program_name) const;
        [[nodiscard]] const ShaderProgram* findProgram(const String& program_name, const String& variant_name) const;

    private:
        UnorderedMap<String, MaterialShader> m_shaders{};
    };

} // namespace dodoe
