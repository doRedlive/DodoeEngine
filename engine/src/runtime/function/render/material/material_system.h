#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/render/shader/shader_reflection.h"

namespace dodoe {

    class ShaderLibrary;
    class BindingLayoutCache;
    class BindingSetCache;
    class TextureManager;
    class Texture2D;
    class DrawCommandList;

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

        [[nodiscard]] Size_t computeHash() const;
        [[nodiscard]] Bool operator==(const MaterialTemplateDesc&) const = default;
    };

    struct MaterialInstanceDesc {
        String name;
        String template_name;

        UnorderedMap<String, MaterialParamValue> param_overrides;
        UnorderedMap<String, UInt32> permutation_overrides;
    };

    struct MaterialTemplate {
        MaterialTemplateDesc desc;

        GfxShaderHandle vertex_shader{};
        GfxShaderHandle pixel_shader{};
        GfxBindingLayoutHandle binding_layout{};

        UInt64 revision{0};
        Bool resolved{false};
    };

    struct MaterialInstance {
        MaterialInstanceDesc desc;
        const MaterialTemplate* tpl{nullptr};

        DynamicArray<GfxTextureHandle> textures;
        DynamicArray<Int32> texture_descriptor_indices;
        GfxSamplerHandle sampler{};
        GfxBindingSetHandle texture_binding_set{};

        UInt64 revision{0};
        Bool resolved{false};
    };

    struct ResolvedMaterial {
        GfxShaderHandle vertex_shader{};
        GfxShaderHandle pixel_shader{};
        GfxBindingLayoutHandle binding_layout{};

        GfxRasterState rasterizer{};
        GfxDepthStencilState depth_stencil{};
        GfxBlendState blend{};

        DynamicArray<GfxTextureHandle> textures;
        GfxSamplerHandle sampler{};

        DynamicArray<UInt8> parameter_data;

        UInt64 revision{0};

        [[nodiscard]] Bool valid() const {
            return vertex_shader && pixel_shader && binding_layout;
        }
    };

    class MaterialSystem {
    public:
        MaterialSystem() = default;

        void initialize(ShaderLibrary* shader_library,
                        BindingLayoutCache* binding_layout_cache,
                        BindingSetCache* binding_set_cache,
                        TextureManager* texture_manager);

        void registerBuiltinTemplates();
        void shutdown();

        Bool registerTemplate(const MaterialTemplateDesc& desc);
        const MaterialTemplate* findTemplate(const String& name) const;
        const MaterialTemplate* findTemplateByShader(const String& shader_name) const;

        Bool createInstance(const MaterialInstanceDesc& desc);
        const MaterialInstance* findInstance(const String& name) const;

        const MaterialInstance* getOrCreateInstance(const String& name,
                                                    const String& template_name,
                                                    const UnorderedMap<String, MaterialParamValue>& param_overrides);

        void setInstanceParam(const String& instance_name,
                              const String& param_name,
                              MaterialParamValue value);

        Bool resolveTemplate(const String& name);
        Bool resolveInstance(const String& name);
        void resolveAll();

        Bool getResolvedMaterial(const String& instance_name,
                                 const UnorderedMap<String, UInt32>& permutation_overrides,
                                 ResolvedMaterial& out_material);

        Bool buildConstantBufferData(const String& instance_name,
                                     const ShaderCBReflection& cb_reflection,
                                     DynamicArray<UInt8>& out_data) const;

        void invalidateForShader(const String& shader_name);
        void invalidateForTexture(const GfxTextureHandle& texture);
        void invalidateAll();

        const UnorderedMap<String, MaterialTemplate>& getTemplates() const { return m_templates; }
        const UnorderedMap<String, MaterialInstance>& getInstances() const { return m_instances; }
        [[nodiscard]] UInt64 getGlobalRevision() const { return m_global_revision; }

    private:
        void getResolvedParams(const String& instance_name,
                               UnorderedMap<String, MaterialParamValue>& out_params) const;

        Bool resolveTextureSlot(MaterialInstance& instance,
                                const MaterialParamDef& def,
                                MaterialParamValue value);

        Texture2D* findTexture2DByHandle(GfxTextureHandle handle) const;

        UnorderedMap<String, MaterialTemplate> m_templates{};
        UnorderedMap<String, MaterialInstance> m_instances{};

        ShaderLibrary* m_shader_library{nullptr};
        BindingLayoutCache* m_binding_layout_cache{nullptr};
        BindingSetCache* m_binding_set_cache{nullptr};
        TextureManager* m_texture_manager{nullptr};

        UInt64 m_global_revision{0};
    };

} // namespace dodoe
