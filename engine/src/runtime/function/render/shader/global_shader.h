// do@Redlive

#pragma once

#include "runtime/function/render/shader/shader_program.h"

namespace dodoe {

    class GlobalShader {
    public:
        explicit GlobalShader(String name = {});

        [[nodiscard]] const String& getName() const;
        [[nodiscard]] const ShaderProgram& getProgram() const;
        [[nodiscard]] ShaderProgram& getProgram();

    private:
        ShaderProgram m_program{};
    };

    class GlobalShaderMap {
    public:
        void clear();
        void registerStage(const String& program_name, GfxShaderType stage, const GfxShaderHandle& shader, const ShaderReflectionData* reflection);

        [[nodiscard]] const GlobalShader* find(const String& program_name) const;
        [[nodiscard]] const ShaderProgram* findProgram(const String& program_name) const;

    private:
        UnorderedMap<String, GlobalShader> m_shaders{};
    };

} // namespace dodoe
