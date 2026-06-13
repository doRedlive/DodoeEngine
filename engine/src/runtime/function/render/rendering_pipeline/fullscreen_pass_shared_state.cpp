#include "fullscreen_pass_shared_state.h"

namespace dodoe {
    namespace {
        constexpr UInt32 kDeferredLightConstantBufferVersions = 128;

        GfxBindingLayoutHandle CreateSingleInputBindingLayout(IDevice* device) {
            DO_ASSERT(device != nullptr, "SingleInputPass device is null");
            return device->createBindingLayout(
                BindingLayoutDesc()
                    .setVisibility(ShaderType::All)
                    .addItem(BindingLayoutItem::Texture_SRV(0))
                    .addItem(BindingLayoutItem::Sampler(0))
            );
        }

        GfxBindingLayoutHandle CreateSkyboxBindingLayout(IDevice* device) {
            DO_ASSERT(device != nullptr, "SkyboxPass device is null");
            return device->createBindingLayout(
                BindingLayoutDesc()
                    .setVisibility(ShaderType::All)
                    .addItem(BindingLayoutItem::PushConstants(0, sizeof(Matrix4f)))
                    .addItem(BindingLayoutItem::Texture_SRV(0))
                    .addItem(BindingLayoutItem::Texture_SRV(1))
                    .addItem(BindingLayoutItem::Sampler(0))
            );
        }

        GfxBindingLayoutHandle CreateDeferredLightBindingLayout(IDevice* device) {
            DO_ASSERT(device != nullptr, "DeferredLightPass device is null");
            return device->createBindingLayout(
                BindingLayoutDesc()
                    .setVisibility(ShaderType::Pixel)
                    .addItem(BindingLayoutItem::VolatileConstantBuffer(0))
                    .addItem(BindingLayoutItem::Sampler(0))
                    .addItem(BindingLayoutItem::Texture_SRV(0))
                    .addItem(BindingLayoutItem::Texture_SRV(1))
                    .addItem(BindingLayoutItem::Texture_SRV(2))
                    .addItem(BindingLayoutItem::Texture_SRV(3))
                    .addItem(BindingLayoutItem::Texture_SRV(4))
                    .addItem(BindingLayoutItem::Texture_SRV(5))
            );
        }

        GfxBindingLayoutHandle CreateColorGradingBindingLayout(IDevice* device) {
            DO_ASSERT(device != nullptr, "ColorGradingPass device is null");
            return device->createBindingLayout(
                BindingLayoutDesc()
                    .setVisibility(ShaderType::All)
                    .addItem(BindingLayoutItem::PushConstants(0, sizeof(Vector4f)))
                    .addItem(BindingLayoutItem::Texture_SRV(0))
                    .addItem(BindingLayoutItem::Sampler(0))
            );
        }

        GfxBindingLayoutHandle CreatePresentBindingLayout(IDevice* device) {
            DO_ASSERT(device != nullptr, "PresentPass device is null");
            return device->createBindingLayout(
                BindingLayoutDesc()
                    .setVisibility(ShaderType::All)
                    .addItem(BindingLayoutItem::PushConstants(0, sizeof(float) * 4))
                    .addItem(BindingLayoutItem::Texture_SRV(0))
                    .addItem(BindingLayoutItem::Texture_SRV(1))
                    .addItem(BindingLayoutItem::Sampler(0))
            );
        }
    } // namespace

    Bool FullscreenPassSharedState::initialize(GfxContext& gfx_context) {
        const auto device = gfx_context.getDevice();
        DO_ASSERT(device != nullptr, "FullscreenPassSharedState device is null");

        m_screen_sampler = device->createSampler(SamplerDesc());
        m_single_input_binding_layout = CreateSingleInputBindingLayout(device);
        m_skybox_binding_layout = CreateSkyboxBindingLayout(device);
        m_deferred_light_binding_layout = CreateDeferredLightBindingLayout(device);
        m_color_grading_binding_layout = CreateColorGradingBindingLayout(device);
        m_present_binding_layout = CreatePresentBindingLayout(device);
        m_deferred_light_constant_buffer = device->createBuffer(
            BufferDesc()
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
