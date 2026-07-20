// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/shader/shader_manifest.h"
#include "runtime/function/render/shader/shader_reflection.h"

namespace dodoe {

    class ShaderLibrary {
        UnorderedMap<String, GfxShaderHandle> m_shaders{};
        UnorderedMap<String, ShaderReflectionData> m_reflections{};
        ShaderManifest m_manifest{};

    public:
        void initialize(GfxContext& gfx_context);
        void reset();

        const GfxShaderHandle* findShader(const String& name) const;
        const ShaderReflectionData* getReflection(const String& name) const;
        const ShaderManifest& getManifest() const { return m_manifest; }

        GfxShaderHandle getGBufferVertexShader() const { return findShaderValue("GBufferVS"); }
        GfxShaderHandle getGBufferPixelShader() const { return findShaderValue("GBufferPS"); }
        GfxShaderHandle getShadowVertexShader() const { return findShaderValue("ShadowVS"); }
        GfxShaderHandle getShadowPixelShader() const { return findShaderValue("ShadowPS"); }
        GfxShaderHandle getFullscreenVertexShader() const { return findShaderValue("FullscreenVS"); }
        GfxShaderHandle getSkyboxPixelShader() const { return findShaderValue("SkyboxPS"); }
        GfxShaderHandle getDeferredLightPixelShader() const { return findShaderValue("DeferredLightPS"); }
        GfxShaderHandle getToneMappingPixelShader() const { return findShaderValue("ToneMappingPS"); }
        GfxShaderHandle getColorGradingPixelShader() const { return findShaderValue("ColorGradingPS"); }
        GfxShaderHandle getFxaaPixelShader() const { return findShaderValue("FxaaPS"); }
        GfxShaderHandle getPresentPixelShader() const { return findShaderValue("PresentPS"); }
        GfxShaderHandle getImGuiVertexShader() const { return findShaderValue("ImGuiVS"); }
        GfxShaderHandle getImGuiPixelShader() const { return findShaderValue("ImGuiPS"); }
        GfxShaderHandle getSpriteVertexShader() const { return findShaderValue("SpriteVS"); }
        GfxShaderHandle getSpritePixelShader() const { return findShaderValue("SpritePS"); }
        GfxShaderHandle getSpritePixelShaderTraditional() const { return findShaderValue("SpriteTraditionalPS"); }
        GfxShaderHandle getTestVertexShader() const { return findShaderValue("TestVS"); }
        GfxShaderHandle getTestPixelShader() const { return findShaderValue("TestPS"); }
        GfxShaderHandle getGpuCullingComputeShader() const { return findShaderValue("GpuCullingCS"); }
        GfxShaderHandle getBucketCountComputeShader() const { return findShaderValue("BucketCountCS"); }
        GfxShaderHandle getBucketFillComputeShader() const { return findShaderValue("BucketFillCS"); }
        GfxShaderHandle getGizmoVertexShader() const { return findShaderValue("GizmoVS"); }
        GfxShaderHandle getGizmoPixelShader() const { return findShaderValue("GizmoPS"); }

    private:
        GfxShaderHandle findShaderValue(const String& name) const {
            const auto* shader = findShader(name);
            return shader ? *shader : GfxShaderHandle{};
        }
    };

} // namespace dodoe
