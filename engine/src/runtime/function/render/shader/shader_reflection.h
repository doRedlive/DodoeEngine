// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    enum class ShaderResourceKind : UInt8 {
        TextureSRV,
        TextureUAV,
        TypedBufferSRV,
        TypedBufferUAV,
        StructuredBufferSRV,
        StructuredBufferUAV,
        RawBufferSRV,
        RawBufferUAV,
        ConstantBuffer,
        VolatileConstantBuffer,
        Sampler,
        RayTracingAccelStruct,
        PushConstants,
        SamplerFeedbackTextureUAV,
    };

    struct ShaderCBVariable {
        String name;
        UInt32 offset;
        UInt32 size;
    };

    struct ShaderCBReflection {
        String name;
        UInt32 slot;
        UInt32 size;
        DynamicArray<ShaderCBVariable> variables;
    };

    struct ShaderTextureReflection {
        String name;
        UInt32 slot;
        ShaderResourceKind kind;
        GfxTextureDimension dimension;
        UInt32 array_size{1};
    };

    struct ShaderSamplerReflection {
        String name;
        UInt32 slot;
    };

    struct ShaderVertexInput {
        String semantic_name;
        UInt32 semantic_index;
        GfxFormat format;
        UInt32 location;
    };

    struct ShaderReflectionData {
        String shader_name;
        GfxShaderType stage;

        DynamicArray<ShaderCBReflection> constant_buffers;
        DynamicArray<ShaderTextureReflection> textures;
        DynamicArray<ShaderSamplerReflection> samplers;
        DynamicArray<ShaderVertexInput> vertex_inputs;

        Bool uses_push_constants{false};
        UInt32 push_constant_size{0};

        Bool valid() const { return stage != GfxShaderType::None; }
    };

    class ShaderReflector {
    public:
        static ShaderReflectionData Reflect(const GfxShaderHandle& shader, const String& name);

        static ShaderReflectionData ReflectBytecode(const DynamicArray<UInt8>& bytecode,
                                                    GfxShaderType stage,
                                                    const String& name);

        static Bool ValidateAgainstLayout(const ShaderReflectionData& reflection,
                                          const GfxBindingLayoutDesc& layout,
                                          String& out_error);

    private:
        static ShaderReflectionData ReflectSPIRV(const DynamicArray<UInt8>& bytecode,
                                                 GfxShaderType stage,
                                                 const String& name);

        static Bool IsSPIRV(const DynamicArray<UInt8>& bytecode);
    };

    GfxBindingLayoutItem ShaderResourceKindToBindingItem(ShaderResourceKind kind, UInt32 slot, UInt32 array_size = 1);

} // namespace dodoe
