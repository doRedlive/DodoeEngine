// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/render/shader/shader_reflection.h"

namespace dodoe {
    class DrawCommandList;

    struct GeneratedBindingLayout {
        GfxBindingLayoutDesc desc;
        Bool valid{false};
    };

    class BindingLayoutGenerator {
    public:
        static GeneratedBindingLayout Generate(const ShaderReflectionData& reflection);

        static GeneratedBindingLayout Generate(const ShaderReflectionData& vs_reflection,
                                               const ShaderReflectionData& ps_reflection);

        static GeneratedBindingLayout Generate(const DynamicArray<const ShaderReflectionData*>& stages);

        static GfxBindingLayoutHandle CreateLayout(const GeneratedBindingLayout& generated,
                                                   GfxDeviceHandle device);

        static GfxBindingLayoutHandle CreateLayout(const GeneratedBindingLayout& generated,
                                                   DrawCommandList& cmd_list);

    private:
        struct MergedBinding {
            UInt32 slot;
            ShaderResourceKind kind;
            GfxShaderType visibility;
            UInt32 array_size;
            Bool operator==(const MergedBinding& other) const {
                return slot == other.slot && kind == other.kind;
            }
        };

        static void MergeReflection(const ShaderReflectionData& refl, DynamicArray<MergedBinding>& bindings);
    };

} // namespace dodoe
