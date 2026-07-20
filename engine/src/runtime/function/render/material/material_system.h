// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/render/shader/shader_reflection.h"

namespace dodoe {

    enum class MaterialParamType : UInt8 {
        Float,
        Float2,
        Float3,
        Float4,
        Color3,
        Color4,
        Texture2D,
        TextureCube,
        Int,
        Bool,
    };

    union MaterialParamValue {
        Float f[4];
        Int32 i[4];
        GfxTextureHandle texture;

        MaterialParamValue() : f{0.0f, 0.0f, 0.0f, 0.0f} {}
    };

    struct MaterialParamDef {
        String name;
        String display_name;
        MaterialParamType type;
        MaterialParamValue default_value;
        MaterialParamValue min_value;
        MaterialParamValue max_value;
    };

    struct MaterialTemplateDesc {
        String name;
        String shader_name;

        GfxRasterState rasterizer{};
        GfxDepthStencilState depth_stencil{};
        GfxBlendState blend{};

        DynamicArray<MaterialParamDef> param_defs;

        UnorderedMap<String, UInt32> permutation_defaults;
    };

    struct MaterialInstanceDesc {
        String name;
        String template_name;

        UnorderedMap<String, MaterialParamValue> param_overrides;
        UnorderedMap<String, UInt32> permutation_overrides;
    };

    class MaterialSystem {
        UnorderedMap<String, MaterialTemplateDesc> m_templates;
        UnorderedMap<String, MaterialInstanceDesc> m_instances;

    public:
        Bool registerTemplate(const MaterialTemplateDesc& desc);
        const MaterialTemplateDesc* findTemplate(const String& name) const;

        Bool createInstance(const MaterialInstanceDesc& desc);
        const MaterialInstanceDesc* findInstance(const String& name) const;

        void setInstanceParam(const String& instance_name,
                              const String& param_name,
                              MaterialParamValue value);

        void getResolvedParams(const String& instance_name,
                               UnorderedMap<String, MaterialParamValue>& out_params) const;

        Bool buildConstantBufferData(const String& instance_name,
                                     const ShaderCBReflection& cb_reflection,
                                     DynamicArray<UInt8>& out_data) const;

        const UnorderedMap<String, MaterialTemplateDesc>& getTemplates() const { return m_templates; }
        const UnorderedMap<String, MaterialInstanceDesc>& getInstances() const { return m_instances; }
    };

} // namespace dodoe
