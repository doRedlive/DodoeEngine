#include "fullscreen_pass_shared_state.h"
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {
    namespace {
        constexpr UInt32 kDeferredLightConstantBufferVersions = 128;

        GfxBindingLayoutHandle CreateSingleInputBindingLayout(GfxDevice* device) {
            DO_ASSERT(device != nullptr, "SingleInputPass device is null");
            return device->createBindingLayout(
                GfxBindingLayoutDesc()
                    .setVisibility(GfxShaderType::All)
                    .addItem(GfxBindingLayoutItem::Texture_SRV(0))
                    .addItem(GfxBindingLayoutItem::Sampler(0))
            );
        }

        GfxBindingLayoutHandle CreateSkyboxBindingLayout(GfxDevice* device) {
            DO_ASSERT(device != nullptr, "SkyboxPass device is null");
            return device->createBindingLayout(
                GfxBindingLayoutDesc()
                    .setVisibility(GfxShaderType::All)
                    .addItem(GfxBindingLayoutItem::PushConstants(0, sizeof(Matrix4f)))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(0))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                    .addItem(GfxBindingLayoutItem::Sampler(0))
            );
        }

        GfxBindingLayoutHandle CreateDeferredLightBindingLayout(GfxDevice* device) {
            DO_ASSERT(device != nullptr, "DeferredLightPass device is null");
            return device->createBindingLayout(
                GfxBindingLayoutDesc()
                    .setVisibility(GfxShaderType::Pixel)
                    .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(0))
                    .addItem(GfxBindingLayoutItem::Sampler(0))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(0))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(2))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(3))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(4))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(5))
            );
        }

        GfxBindingLayoutHandle CreateColorGradingBindingLayout(GfxDevice* device) {
            DO_ASSERT(device != nullptr, "ColorGradingPass device is null");
            return device->createBindingLayout(
                GfxBindingLayoutDesc()
                    .setVisibility(GfxShaderType::All)
                    .addItem(GfxBindingLayoutItem::PushConstants(0, sizeof(Vector4f)))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(0))
                    .addItem(GfxBindingLayoutItem::Sampler(0))
            );
        }

        GfxBindingLayoutHandle CreatePresentBindingLayout(GfxDevice* device) {
            DO_ASSERT(device != nullptr, "PresentPass device is null");
            return device->createBindingLayout(
                GfxBindingLayoutDesc()
                    .setVisibility(GfxShaderType::All)
                    .addItem(GfxBindingLayoutItem::PushConstants(0, sizeof(float) * 4))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(0))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                    .addItem(GfxBindingLayoutItem::Sampler(0))
            );
        }
    } // namespace

    Bool FullscreenPassSharedState::initialize(GfxContext& gfx_context) {
        const auto device = gfx_context.getDevice();
        DO_ASSERT(device != nullptr, "FullscreenPassSharedState device is null");

        m_screen_sampler = device->createSampler(GfxSamplerDesc());
        m_single_input_binding_layout = CreateSingleInputBindingLayout(device);
        m_skybox_binding_layout = CreateSkyboxBindingLayout(device);
        m_deferred_light_binding_layout = CreateDeferredLightBindingLayout(device);
        m_color_grading_binding_layout = CreateColorGradingBindingLayout(device);
        m_present_binding_layout = CreatePresentBindingLayout(device);
        m_deferred_light_constant_buffer = device->createBuffer(
            GfxBufferDesc()
                .setByteSize(256)
                .setIsConstantBuffer(true)
                .setIsVolatile(true)
                .setMaxVersions(kDeferredLightConstantBufferVersions)
                .setDebugName("DeferredLightPass ConstantBuffer")
        );
        return true;
    }

    void FullscreenPassSharedState::reset() {
        m_deferred_light_constant_buffer = nullptr;
        m_present_binding_layout = nullptr;
        m_color_grading_binding_layout = nullptr;
        m_deferred_light_binding_layout = nullptr;
        m_skybox_binding_layout = nullptr;
        m_single_input_binding_layout = nullptr;
        m_screen_sampler = nullptr;
    }

} // dodoe
