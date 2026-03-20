//
// Created by Redlive on 2026/3/18.
//

#include "vk_shader.h"

namespace {
    VkShaderModule create_vk_shader_module(VkDevice device, const dodoe::ShaderModuleCreateInfo& module_info) {
        DoAssert(module_info.code, "VkShader::initialize: shader module code must not be null.");
        DoAssert(module_info.code_size > 0, "VkShader::initialize: shader module code size must be greater than zero.");
        DoAssert(module_info.code_type == dodoe::ShaderCodeType::SPIRV,
            "VkShader::initialize: Vulkan backend only supports SPIR-V module code.");
        DoAssert((module_info.code_size % 4) == 0,
            "VkShader::initialize: SPIR-V code size must be a multiple of 4 bytes.");

        VkShaderModuleCreateInfo shader_module_create_info{};
        shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_module_create_info.codeSize = static_cast<size_t>(module_info.code_size);
        shader_module_create_info.pCode = reinterpret_cast<const uint32_t*>(module_info.code);

        VkShaderModule shader_module{VK_NULL_HANDLE};
        const VkResult result = vkCreateShaderModule(device, &shader_module_create_info, nullptr, &shader_module);
        DoAssert(result == VK_SUCCESS, "VkShader::initialize: vkCreateShaderModule failed.");

        return shader_module;
    }
}

namespace dodoe {

    void VkShader::attach() {
        // Vulkan shader modules are consumed by pipeline creation; no direct runtime attach step.
    }

    void VkShader::detach() {
        // Vulkan shader modules are consumed by pipeline creation; no direct runtime detach step.
    }

    void VkShader::initialize(ShaderCreateInfo create_info) {
        DoAssert(create_info.native_device, "ShaderCreateInfo::native_device must not be null for Vulkan shaders.");
        DoAssert(create_info.vert_module.stage == ShaderStage::Vertex,
            "VkShader::initialize: vert_module.stage must be ShaderStage::Vertex.");
        DoAssert(create_info.frag_module.stage == ShaderStage::Fragment,
            "VkShader::initialize: frag_module.stage must be ShaderStage::Fragment.");

        device_ = reinterpret_cast<VkDevice>(create_info.native_device);
        vert_module_ = create_vk_shader_module(device_, create_info.vert_module);
        frag_module_ = create_vk_shader_module(device_, create_info.frag_module);
    }

    void VkShader::shutdown() {
        if (device_ == VK_NULL_HANDLE) {
            return;
        }

        if (vert_module_ != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, vert_module_, nullptr);
            vert_module_ = VK_NULL_HANDLE;
        }

        if (frag_module_ != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, frag_module_, nullptr);
            frag_module_ = VK_NULL_HANDLE;
        }

        device_ = VK_NULL_HANDLE;
    }

} // dodoe
