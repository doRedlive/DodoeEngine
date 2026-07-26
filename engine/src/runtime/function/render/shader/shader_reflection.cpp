// do@Redlive

#include "shader_reflection.h"

#include <directx/d3d12shader.h>
#include <wrl/client.h>

#pragma comment(lib, "d3dcompiler.lib")

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

    Bool ShaderReflector::IsDXIL(const DynamicArray<UInt8>& bytecode) {
        if (bytecode.size() < 4) {
            return false;
        }
        return bytecode[0] == 'D' && bytecode[1] == 'X' && bytecode[2] == 'B' && bytecode[3] == 'C';
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
        if (IsDXIL(bytecode)) {
            return ReflectDXIL(bytecode, stage, name);
        }
        if (IsSPIRV(bytecode)) {
            return ReflectSPIRV(bytecode, stage, name);
        }

        DO_ERROR("ShaderReflector::ReflectBytecode unknown bytecode format for {}", name);
        return {};
    }

    ShaderReflectionData ShaderReflector::ReflectDXIL(const DynamicArray<UInt8>& bytecode,
                                                      GfxShaderType stage,
                                                      const String& name) {
        ShaderReflectionData result;
        result.shader_name = name;
        result.stage = stage;

        Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;
        HRESULT hr = D3DReflect(bytecode.data(), bytecode.size(),
                                IID_PPV_ARGS(&reflection));

        if (FAILED(hr) || !reflection) {
            DO_ERROR("ShaderReflector::ReflectDXIL D3DReflect failed for {}", name);
            return result;
        }

        D3D12_SHADER_DESC shader_desc{};
        reflection->GetDesc(&shader_desc);

        result.constant_buffers.reserve(shader_desc.ConstantBuffers);
        for (UINT c = 0; c < shader_desc.ConstantBuffers; ++c) {
            ID3D12ShaderReflectionConstantBuffer* cb = reflection->GetConstantBufferByIndex(c);
            D3D12_SHADER_BUFFER_DESC cb_desc{};
            cb->GetDesc(&cb_desc);

            ShaderCBReflection cb_refl;
            cb_refl.name = cb_desc.Name ? cb_desc.Name : "";
            cb_refl.size = cb_desc.Size;

            for (UINT v = 0; v < cb_desc.Variables; ++v) {
                ID3D12ShaderReflectionVariable* var = cb->GetVariableByIndex(v);
                D3D12_SHADER_VARIABLE_DESC var_desc{};
                var->GetDesc(&var_desc);

                ShaderCBVariable cv;
                cv.name = var_desc.Name ? var_desc.Name : "";
                cv.offset = var_desc.StartOffset;
                cv.size = var_desc.Size;
                cb_refl.variables.push_back(std::move(cv));
            }

            result.constant_buffers.push_back(std::move(cb_refl));
        }

        for (UINT c = 0; c < shader_desc.ConstantBuffers; ++c) {
            ID3D12ShaderReflectionConstantBuffer* cb = reflection->GetConstantBufferByIndex(c);
            D3D12_SHADER_BUFFER_DESC cb_desc{};
            cb->GetDesc(&cb_desc);
            result.constant_buffers[static_cast<Size_t>(c)].slot = cb_desc.BindPoint;
        }

        result.textures.reserve(shader_desc.BoundResources);
        for (UINT i = 0; i < shader_desc.BoundResources; ++i) {
            D3D12_SHADER_INPUT_BIND_DESC bind_desc{};
            reflection->GetResourceBindingDesc(i, &bind_desc);

            ShaderTextureReflection tex;
            tex.name = bind_desc.Name ? bind_desc.Name : "";
            tex.slot = bind_desc.BindPoint;
            tex.array_size = bind_desc.BindCount > 0 ? bind_desc.BindCount : 1;

            switch (bind_desc.Type) {
                case D3D_SIT_TEXTURE: {
                    switch (bind_desc.Dimension) {
                        case D3D_SRV_DIMENSION_TEXTURE2D:       tex.dimension = GfxTextureDimension::Texture2D;       break;
                        case D3D_SRV_DIMENSION_TEXTURE2DARRAY:  tex.dimension = GfxTextureDimension::Texture2DArray;  break;
                        case D3D_SRV_DIMENSION_TEXTURE3D:       tex.dimension = GfxTextureDimension::Texture3D;       break;
                        case D3D_SRV_DIMENSION_TEXTURECUBE:     tex.dimension = GfxTextureDimension::TextureCube;     break;
                        case D3D_SRV_DIMENSION_TEXTURECUBEARRAY:tex.dimension = GfxTextureDimension::TextureCubeArray;break;
                        default:                                tex.dimension = GfxTextureDimension::Unknown;         break;
                    }
                    tex.kind = bind_desc.BindCount > 0 && (bind_desc.uFlags & D3D_SIF_UNORDERED_ACCESS)
                        ? ShaderResourceKind::TextureUAV : ShaderResourceKind::TextureSRV;
                    result.textures.push_back(tex);
                    break;
                }
                case D3D_SIT_BYTEADDRESS: {
                    tex.dimension = GfxTextureDimension::Unknown;
                    tex.kind = ShaderResourceKind::RawBufferSRV;
                    result.textures.push_back(tex);
                    break;
                }
                case D3D_SIT_STRUCTURED: {
                    tex.dimension = GfxTextureDimension::Unknown;
                    tex.kind = (bind_desc.uFlags & D3D_SIF_UNORDERED_ACCESS)
                        ? ShaderResourceKind::StructuredBufferUAV : ShaderResourceKind::StructuredBufferSRV;
                    result.textures.push_back(tex);
                    break;
                }
                case D3D_SIT_UAV_RWTYPED: {
                    tex.dimension = GfxTextureDimension::Unknown;
                    tex.kind = ShaderResourceKind::TypedBufferUAV;
                    result.textures.push_back(tex);
                    break;
                }
                case D3D_SIT_UAV_RWBYTEADDRESS: {
                    tex.dimension = GfxTextureDimension::Unknown;
                    tex.kind = ShaderResourceKind::RawBufferUAV;
                    result.textures.push_back(tex);
                    break;
                }
                case D3D_SIT_UAV_RWSTRUCTURED:
                case D3D_SIT_UAV_APPEND_STRUCTURED:
                case D3D_SIT_UAV_CONSUME_STRUCTURED:
                case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: {
                    tex.dimension = GfxTextureDimension::Unknown;
                    tex.kind = ShaderResourceKind::StructuredBufferUAV;
                    result.textures.push_back(tex);
                    break;
                }
                case D3D_SIT_SAMPLER: {
                    ShaderSamplerReflection smp;
                    smp.name = bind_desc.Name ? bind_desc.Name : "";
                    smp.slot = bind_desc.BindPoint;
                    result.samplers.push_back(smp);
                    break;
                }
                case D3D_SIT_CBUFFER:
                case D3D_SIT_TBUFFER:
                    break;
                default:
                    break;
            }
        }

        result.vertex_inputs.reserve(shader_desc.InputParameters);
        for (UINT i = 0; i < shader_desc.InputParameters; ++i) {
            D3D12_SIGNATURE_PARAMETER_DESC sig_desc{};
            reflection->GetInputParameterDesc(i, &sig_desc);

            ShaderVertexInput vi;
            vi.semantic_name = sig_desc.SemanticName ? sig_desc.SemanticName : "";
            vi.semantic_index = sig_desc.SemanticIndex;
            vi.location = i;

            if (sig_desc.Mask == 1) {
                switch (sig_desc.ComponentType) {
                    case D3D_REGISTER_COMPONENT_UINT32:  vi.format = GfxFormat::R32_UINT;   break;
                    case D3D_REGISTER_COMPONENT_SINT32:  vi.format = GfxFormat::R32_SINT;   break;
                    case D3D_REGISTER_COMPONENT_FLOAT32: vi.format = GfxFormat::R32_FLOAT;  break;
                    default:                             vi.format = GfxFormat::UNKNOWN;     break;
                }
            } else if (sig_desc.Mask <= 3) {
                switch (sig_desc.ComponentType) {
                    case D3D_REGISTER_COMPONENT_UINT32:  vi.format = GfxFormat::RG32_UINT;  break;
                    case D3D_REGISTER_COMPONENT_SINT32:  vi.format = GfxFormat::RG32_SINT;  break;
                    case D3D_REGISTER_COMPONENT_FLOAT32: vi.format = GfxFormat::RG32_FLOAT; break;
                    default:                             vi.format = GfxFormat::UNKNOWN;     break;
                }
            } else if (sig_desc.Mask <= 7) {
                switch (sig_desc.ComponentType) {
                    case D3D_REGISTER_COMPONENT_UINT32:  vi.format = GfxFormat::RGB32_UINT;  break;
                    case D3D_REGISTER_COMPONENT_SINT32:  vi.format = GfxFormat::RGB32_SINT;  break;
                    case D3D_REGISTER_COMPONENT_FLOAT32: vi.format = GfxFormat::RGB32_FLOAT; break;
                    default:                             vi.format = GfxFormat::UNKNOWN;      break;
                }
            } else {
                switch (sig_desc.ComponentType) {
                    case D3D_REGISTER_COMPONENT_UINT32:  vi.format = GfxFormat::RGBA32_UINT;  break;
                    case D3D_REGISTER_COMPONENT_SINT32:  vi.format = GfxFormat::RGBA32_SINT;  break;
                    case D3D_REGISTER_COMPONENT_FLOAT32: vi.format = GfxFormat::RGBA32_FLOAT; break;
                    default:                             vi.format = GfxFormat::UNKNOWN;       break;
                }
            }
            result.vertex_inputs.push_back(vi);
        }

        return result;
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

        const auto words = reinterpret_cast<const UInt32*>(bytecode.data());
        const Size_t word_count = bytecode.size() / 4;

        UnorderedMap<UInt32, String> debug_names;
        UnorderedMap<UInt32, UInt32> pointer_types;
        UnorderedMap<UInt32, UInt32> variable_types;
        UnorderedMap<UInt32, UInt32> type_array_element;
        UnorderedMap<UInt32, Bool> struct_is_block;
        UnorderedMap<UInt32, DynamicArray<UInt32>> struct_members;
        UnorderedMap<UInt32, UInt32> type_image_dim;
        UnorderedMap<UInt32, UInt32> type_sampler;
        UnorderedMap<UInt32, UInt32> binding_ids;
        UnorderedMap<UInt32, Bool> image_is_uav;

        Size_t pos = 5;
        while (pos < word_count) {
            UInt32 inst_word = words[pos];
            UInt32 word_len = inst_word & 0xFFFF;
            UInt32 opcode = (inst_word >> 16) & 0xFFFF;

            if (word_len == 0 || pos + word_len > word_count) {
                break;
            }

            switch (opcode) {
                case 5:  {
                    if (word_len >= 2) {
                        UInt32 result_id = words[pos + 1];
                        if (pos + 2 < word_count) {
                            debug_names[result_id] = reinterpret_cast<const char*>(&words[pos + 2]);
                        }
                    }
                    break;
                }
                case 6:  {
                    if (word_len >= 4) {
                        UInt32 type_id = words[pos + 1];
                        UInt32 member_idx = words[pos + 2];
                        String member_name;
                        for (Size_t ci = 3; ci < static_cast<Size_t>(word_len); ++ci) {
                            const char* str = reinterpret_cast<const char*>(&words[pos + ci]);
                            member_name += str;
                        }
                        (void)type_id;
                        (void)member_idx;
                        (void)member_name;
                    }
                    break;
                }
                case 71:  {
                    if (word_len >= 3) {
                        UInt32 target_id = words[pos + 1];
                        UInt32 decoration = words[pos + 2];
                        if (decoration == 33 && word_len >= 4) {
                            binding_ids[target_id] = words[pos + 3];
                        } else if (decoration == 34 && word_len >= 4) {
                            (void)words[pos + 3];
                        } else if (decoration == 1) {
                            struct_is_block[target_id] = true;
                        }
                    }
                    break;
                }
                case 72:  {
                    if (word_len >= 4) {
                        UInt32 type_id = words[pos + 1];
                        UInt32 member_idx = words[pos + 2];
                        UInt32 decoration = words[pos + 3];
                        if (decoration == 35 && word_len >= 5) {
                            if (!struct_members.contains(type_id)) {
                                struct_members[type_id] = DynamicArray<UInt32>();
                            }
                            while (struct_members[type_id].size() <= static_cast<Size_t>(member_idx)) {
                                struct_members[type_id].push_back(0);
                            }
                            struct_members[type_id][static_cast<Size_t>(member_idx)] = words[pos + 4];
                        }
                    }
                    break;
                }
                case 30:  {
                    if (word_len >= 3) {
                        UInt32 result_id = words[pos + 1];
                        UInt32 pointee_type = words[pos + 2];
                        pointer_types[result_id] = pointee_type;
                    }
                    break;
                }
                case 19:  {
                    if (word_len >= 3) {
                        UInt32 result_id = words[pos + 1];
                        if (word_len >= 4) {
                            type_image_dim[result_id] = words[pos + 3];
                        }
                    }
                    break;
                }
                case 26:  {
                    if (word_len >= 2) {
                        UInt32 result_id = words[pos + 1];
                        type_sampler[result_id] = 1;
                    }
                    break;
                }
                case 28:  {
                    if (word_len >= 3) {
                        UInt32 result_id = words[pos + 1];
                        UInt32 element_type = words[pos + 2];
                        type_array_element[result_id] = element_type;
                    }
                    break;
                }
                case 59:  {
                    if (word_len >= 3) {
                        UInt32 result_id = words[pos + 1];
                        UInt32 id = words[pos + 2];
                        variable_types[result_id] = id;
                    }
                    break;
                }
                case 15:  {
                    for (Size_t oi = 1; oi < static_cast<Size_t>(word_len); ++oi) {
                        UInt32 mode = words[pos + oi];
                        if (mode == 17 && oi + 1 < static_cast<Size_t>(word_len)) {
                            result.push_constant_size = words[pos + oi + 1];
                            result.uses_push_constants = true;
                        }
                    }
                    break;
                }
                default:
                    break;
            }

            pos += word_len;
        }

        for (const auto& [var_id, type_id] : variable_types) {
            UInt32 slot = 0;
            auto binding_it = binding_ids.find(var_id);
            if (binding_it != binding_ids.end()) {
                slot = binding_it->second;
            }

            String var_name;
            auto name_it = debug_names.find(var_id);
            if (name_it != debug_names.end()) {
                var_name = name_it->second;
            }

            UInt32 actual_type = type_id;
            auto ptr_it = pointer_types.find(type_id);
            if (ptr_it != pointer_types.end()) {
                actual_type = ptr_it->second;
            }

            if (type_image_dim.contains(actual_type)) {
                ShaderTextureReflection tex;
                tex.name = var_name;
                tex.slot = slot;

                UInt32 dim = type_image_dim[actual_type];
                switch (dim) {
                    case 1:  tex.dimension = GfxTextureDimension::Texture1D;       break;
                    case 2:  tex.dimension = GfxTextureDimension::Texture2D;       break;
                    case 3:  tex.dimension = GfxTextureDimension::Texture3D;       break;
                    case 4:  tex.dimension = GfxTextureDimension::TextureCube;     break;
                    case 5:  tex.dimension = GfxTextureDimension::Texture2DArray;  break;
                    default: tex.dimension = GfxTextureDimension::Unknown;          break;
                }

                tex.kind = image_is_uav.contains(var_id) && image_is_uav[var_id]
                    ? ShaderResourceKind::TextureUAV : ShaderResourceKind::TextureSRV;
                result.textures.push_back(tex);
            } else if (type_sampler.contains(actual_type)) {
                ShaderSamplerReflection smp;
                smp.name = var_name;
                smp.slot = slot;
                result.samplers.push_back(smp);
            } else if (struct_is_block.contains(actual_type)) {
                ShaderCBReflection cb;
                cb.name = var_name;
                cb.slot = slot;

                if (struct_members.contains(actual_type)) {
                    for (Size_t mi = 0; mi < struct_members[actual_type].size(); ++mi) {
                        UInt32 member_offset = struct_members[actual_type][mi];
                        ShaderCBVariable cv;
                        cv.offset = member_offset;
                        cb.variables.push_back(cv);
                    }
                }

                result.constant_buffers.push_back(cb);
            }
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
                    item.size >= 1 &&
                    (item.type == cutie::ResourceType::ConstantBuffer ||
                     item.type == cutie::ResourceType::VolatileConstantBuffer)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                out_error = String(("CBV at slot " + std::to_string(cb.slot) + " (" + cb.name.c_str() + ") not found in layout").c_str());
                return false;
            }
        }

        for (const auto& tex : reflection.textures) {
            Bool found = false;
            for (const auto& item : layout.bindings) {
                if (item.slot == tex.slot) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                out_error = String(("Texture at slot " + std::to_string(tex.slot) + " (" + tex.name.c_str() + ") not found in layout").c_str());
                return false;
            }
        }

        for (const auto& smp : reflection.samplers) {
            Bool found = false;
            for (const auto& item : layout.bindings) {
                if (item.slot == smp.slot &&
                    item.type == cutie::ResourceType::Sampler) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                out_error = String(("Sampler at slot " + std::to_string(smp.slot) + " (" + smp.name.c_str() + ") not found in layout").c_str());
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
