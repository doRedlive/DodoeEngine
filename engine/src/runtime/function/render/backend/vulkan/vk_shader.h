//
// Created by Redlive on 2026/3/18.
//

#ifndef DODOE_VK_SHADER_H
#define DODOE_VK_SHADER_H

#include "dopch.h"

#include "runtime/function/render/backend/shader.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

namespace dodoe {

    class VkShader : public Shader {
    public:
        void attach() override;
        void detach() override;

        void set_bool(const std::string& name, bool value) override;
        void set_int(const std::string& name, int value) override;
        void set_float(const std::string& name, float value) override;
        void set_vec2(const std::string& name, const Vector2f& value) override;
        void set_vec3(const std::string& name, const Vector3f& value) override;
        void set_vec4(const std::string& name, const Vector4f& value) override;
        void set_mat4(const std::string& name, const Matrix4f& value) override;

        [[nodiscard]] VkShaderModule vert_module() const { return vert_module_; }
        [[nodiscard]] VkShaderModule frag_module() const { return frag_module_; }

    protected:
        void initialize(ShaderCreateInfo create_info) override;
        void shutdown() override;

    private:
        VkDevice device_{VK_NULL_HANDLE};
        VkShaderModule vert_module_{VK_NULL_HANDLE};
        VkShaderModule frag_module_{VK_NULL_HANDLE};
    };

} // dodoe

#endif//DODOE_VK_SHADER_H
