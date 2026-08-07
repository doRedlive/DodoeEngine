// do@Redlive

#include "shader_reflection.h"

#include "spirv_reflect.hpp"

namespace dodoe {

    GfxBindingLayoutItem ShaderResourceKindToBindingItem(ShaderResourceKind kind, UInt32 slot, UInt32 array_size) {
        switch (kind) {
            case ShaderResourceKind::TextureSRV:              return GfxBindingLayoutItem::Texture_SRV(slot);
            case ShaderResourceKind::TextureUAV:              return GfxBindingLayoutItem::Texture_UAV(slot);
            case ShaderResourceKind::TypedBufferSRV:          return GfxBindingLayoutItem::TypedBuffer_SRV(slot);
            case ShaderResourceKind::TypedBufferUAV:          return GfxBindingLayoutItem::TypedBuffer_UAV(slot);
            case ShaderResourceKind::StructuredBufferSRV:     return GfxBindingLayoutItem::StructuredBuffer_SRV(slot);
            case ShaderResourceKind::StructuredBufferUAV:     return GfxBindingLayoutItem::StructuredBuffer_UAV(slot);
            case ShaderResourceKind::RawBufferSRV:            return GfxBindingLayoutItem::RawBuffer_SRV(slot);
            case ShaderResourceKind::RawBufferUAV:            return GfxBindingLayoutItem::RawBuffer_UAV(slot);
            case ShaderResourceKind::ConstantBuffer:          return GfxBindingLayoutItem::ConstantBuffer(slot);
            case ShaderResourceKind::VolatileConstantBuffer:  return GfxBindingLayoutItem::VolatileConstantBuffer(slot);
            case ShaderResourceKind::Sampler:                 return GfxBindingLayoutItem::Sampler(slot);
            case ShaderResourceKind::RayTracingAccelStruct:   return GfxBindingLayoutItem::RayTracingAccelStruct(slot);
            case ShaderResourceKind::SamplerFeedbackTextureUAV: return GfxBindingLayoutItem::SamplerFeedbackTexture_UAV(slot);
            case ShaderResourceKind::PushConstants:           return GfxBindingLayoutItem::PushConstants(slot, 0);
        }
        return GfxBindingLayoutItem{};
    }

    Bool ShaderReflector::IsSPIRV(const DynamicArray<UInt8>& bytecode) {
        if (bytecode.size() < 4) {
            return false;
        }
        return bytecode[0] == 0x03 && bytecode[1] == 0x02 && bytecode[2] == 0x23 && bytecode[3] == 0x07;
    }

    ShaderReflectionData ShaderReflector::Reflect(const GfxShaderHandle& shader, const String& name) {
        if (!shader) {
            return {};
        }

        const void* data = nullptr;
        size_t size = 0;
        shader->getBytecode(&data, &size);

        if (!data || size == 0) {
            DO_ERROR("ShaderReflector::Reflect null bytecode for {}", name);
            return {};
        }

        const auto* bytes = static_cast<const UInt8*>(data);
        DynamicArray<UInt8> bytecode(size);
        std::memcpy(bytecode.data(), bytes, size);

        return ReflectBytecode(bytecode, shader->getDesc().shaderType, name);
    }

    ShaderReflectionData ShaderReflector::ReflectBytecode(const DynamicArray<UInt8>& bytecode,
                                                          GfxShaderType stage,
                                                          const String& name) {
        if (IsSPIRV(bytecode)) {
            return ReflectSPIRV(bytecode, stage, name);
        }

        DO_ERROR("ShaderReflector::ReflectBytecode unknown or unsupported bytecode format for {}", name);
        return {};
    }

    ShaderReflectionData ShaderReflector::ReflectSPIRV(const DynamicArray<UInt8>& bytecode,
                                                       GfxShaderType stage,
                                                       const String& name) {
        ShaderReflectionData result;
        result.shader_name = name;
        result.stage = stage;

        if (bytecode.size() < 20 || !IsSPIRV(bytecode)) {
            return result;
        }

        const auto* words = reinterpret_cast<const UInt32*>(bytecode.data());
        const size_t word_count = bytecode.size() / 4;

        spirv_cross::CompilerReflection compiler(words, word_count);
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();

        // Uniform buffers → Constant buffers
        for (const auto& ub : resources.uniform_buffers) {
            ShaderCBReflection cb;
            cb.name = String(ub.name.c_str());
            cb.set = compiler.get_decoration(ub.id, spv::DecorationDescriptorSet);
            cb.slot = compiler.get_decoration(ub.id, spv::DecorationBinding);

            const auto& type = compiler.get_type(ub.type_id);
            cb.size = static_cast<UInt32>(compiler.get_declared_struct_size(type));

            for (UInt32 mi = 0; mi < static_cast<UInt32>(type.member_types.size()); ++mi) {
                ShaderCBVariable cv;
                cv.name = String(compiler.get_member_name(ub.type_id, mi).c_str());
                cv.offset = compiler.type_struct_member_offset(type, mi);
                cv.size = static_cast<UInt32>(compiler.get_declared_struct_member_size(type, mi));
                cb.variables.push_back(cv);
            }

            result.constant_buffers.push_back(std::move(cb));
        }

        // Storage buffers (read-only → StructuredBufferSRV, read-write → StructuredBufferUAV)
        for (const auto& sb : resources.storage_buffers) {
            ShaderTextureReflection tex;
            tex.name = String(sb.name.c_str());
            tex.set = compiler.get_decoration(sb.id, spv::DecorationDescriptorSet);
            tex.slot = compiler.get_decoration(sb.id, spv::DecorationBinding);
            tex.dimension = GfxTextureDimension::Unknown;
            tex.kind = ShaderResourceKind::StructuredBufferSRV;
            // Check for NonWritable / NonReadable decorations to distinguish SRV vs UAV
            bool is_non_writable = compiler.get_buffer_block_flags(sb.id).get(spv::DecorationNonWritable);
            if (!is_non_writable) {
                tex.kind = ShaderResourceKind::StructuredBufferUAV;
            }
            result.textures.push_back(tex);
        }

        // Sampled images (textures)
        for (const auto& si : resources.sampled_images) {
            ShaderTextureReflection tex;
            tex.name = String(si.name.c_str());
            tex.set = compiler.get_decoration(si.id, spv::DecorationDescriptorSet);
            tex.slot = compiler.get_decoration(si.id, spv::DecorationBinding);

            const auto& type = compiler.get_type(si.type_id);
            switch (type.image.dim) {
                case spv::Dim1D: tex.dimension = GfxTextureDimension::Texture1D; break;
                case spv::Dim2D: tex.dimension = type.image.arrayed ? GfxTextureDimension::Texture2DArray : GfxTextureDimension::Texture2D; break;
                case spv::Dim3D: tex.dimension = GfxTextureDimension::Texture3D; break;
                case spv::DimCube: tex.dimension = GfxTextureDimension::TextureCube; break;
                default: tex.dimension = GfxTextureDimension::Unknown; break;
            }
            tex.kind = ShaderResourceKind::TextureSRV;
            result.textures.push_back(tex);
        }

        // Storage images (UAV textures)
        for (const auto& img : resources.storage_images) {
            ShaderTextureReflection tex;
            tex.name = String(img.name.c_str());
            tex.set = compiler.get_decoration(img.id, spv::DecorationDescriptorSet);
            tex.slot = compiler.get_decoration(img.id, spv::DecorationBinding);
            const auto& type = compiler.get_type(img.type_id);
            switch (type.image.dim) {
                case spv::Dim2D: tex.dimension = GfxTextureDimension::Texture2D; break;
                default: tex.dimension = GfxTextureDimension::Unknown; break;
            }
            tex.kind = ShaderResourceKind::TextureUAV;
            result.textures.push_back(tex);
        }

        // Separate samplers
        for (const auto& smp : resources.separate_samplers) {
            ShaderSamplerReflection s;
            s.name = String(smp.name.c_str());
            s.set = compiler.get_decoration(smp.id, spv::DecorationDescriptorSet);
            s.slot = compiler.get_decoration(smp.id, spv::DecorationBinding);
            result.samplers.push_back(s);
        }

        // Push constants
        if (!resources.push_constant_buffers.empty()) {
            result.uses_push_constants = true;
            for (const auto& pc : resources.push_constant_buffers) {
                const auto& type = compiler.get_type(pc.base_type_id);
                result.push_constant_size = static_cast<UInt32>(compiler.get_declared_struct_size(type));
                break;
            }
        }

        // Stage inputs (vertex inputs)
        for (const auto& input : resources.stage_inputs) {
            ShaderVertexInput vi;
            vi.semantic_name = String(input.name.c_str());
            vi.semantic_index = 0;
            vi.location = compiler.get_decoration(input.id, spv::DecorationLocation);

            const auto& type = compiler.get_type(input.type_id);
            switch (type.vecsize) {
                case 1: vi.format = GfxFormat::R32_FLOAT; break;
                case 2: vi.format = GfxFormat::RG32_FLOAT; break;
                case 3: vi.format = GfxFormat::RGB32_FLOAT; break;
                case 4: vi.format = GfxFormat::RGBA32_FLOAT; break;
                default: vi.format = GfxFormat::UNKNOWN; break;
            }
            result.vertex_inputs.push_back(vi);
        }

        return result;
    }

    Bool ShaderReflector::ValidateAgainstLayout(const ShaderReflectionData& reflection,
                                                const GfxBindingLayoutDesc& layout,
                                                String& out_error) {
        for (const auto& cb : reflection.constant_buffers) {
            Bool found = false;
            for (const auto& item : layout.bindings) {
                if (item.slot == cb.slot &&
                    cb.set == layout.registerSpace &&
                    item.size >= 1 &&
                    (item.type == cutie::ResourceType::ConstantBuffer ||
                     item.type == cutie::ResourceType::VolatileConstantBuffer)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                out_error = String(("CBV at set " + std::to_string(cb.set) + " slot " + std::to_string(cb.slot) + " (" + cb.name.c_str() + ") not found in layout").c_str());
                return false;
            }
        }

        for (const auto& tex : reflection.textures) {
            Bool found = false;
            for (const auto& item : layout.bindings) {
                if (item.slot == tex.slot &&
                    tex.set == layout.registerSpace) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                out_error = String(("Texture at set " + std::to_string(tex.set) + " slot " + std::to_string(tex.slot) + " (" + tex.name.c_str() + ") not found in layout").c_str());
                return false;
            }
        }

        for (const auto& smp : reflection.samplers) {
            Bool found = false;
            for (const auto& item : layout.bindings) {
                if (item.slot == smp.slot &&
                    smp.set == layout.registerSpace &&
                    item.type == cutie::ResourceType::Sampler) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                out_error = String(("Sampler at set " + std::to_string(smp.set) + " slot " + std::to_string(smp.slot) + " (" + smp.name.c_str() + ") not found in layout").c_str());
                return false;
            }
        }

        if (reflection.uses_push_constants) {
            Bool found = false;
            for (const auto& item : layout.bindings) {
                if (item.type == cutie::ResourceType::PushConstants) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                out_error = "Push constants used by shader but not in layout";
                return false;
            }
        }

        return true;
    }

} // namespace dodoe
