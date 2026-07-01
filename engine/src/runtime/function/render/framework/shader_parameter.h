// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    struct RenderGraphTextureHandle;
    struct RenderGraphBufferHandle;

    enum class ShaderParamType : uint8_t {
        TextureSRV,
        Sampler,
        PushConstants,
        ConstantBuffer,
    };

    template <ShaderParamType Type, uint32_t Slot, typename ValueT = void>
    struct ShaderParameter;

    template <uint32_t Slot>
    struct ShaderParameter<ShaderParamType::TextureSRV, Slot, RenderGraphTextureHandle> {
        static constexpr ShaderParamType kType = ShaderParamType::TextureSRV;
        static constexpr uint32_t kSlot = Slot;
        using ValueType = RenderGraphTextureHandle;

        RenderGraphTextureHandle value{};

        static GfxBindingLayoutItem makeLayoutItem() {
            return GfxBindingLayoutItem::Texture_SRV(Slot);
        }
        static void addToBindingSet(GfxBindingSetDesc& desc, const GfxTextureHandle& resolved) {
            desc.addItem(GfxBindingSetItem::Texture_SRV(Slot, resolved->getRHIHandle().Get()));
        }
    };

    template <uint32_t Slot>
    struct ShaderParameter<ShaderParamType::TextureSRV, Slot, GfxTextureHandle> {
        static constexpr ShaderParamType kType = ShaderParamType::TextureSRV;
        static constexpr uint32_t kSlot = Slot;
        using ValueType = GfxTextureHandle;

        GfxTextureHandle value{};

        static GfxBindingLayoutItem makeLayoutItem() {
            return GfxBindingLayoutItem::Texture_SRV(Slot);
        }
        static void addToBindingSet(GfxBindingSetDesc& desc, const GfxTextureHandle& resolved) {
            desc.addItem(GfxBindingSetItem::Texture_SRV(Slot, resolved->getRHIHandle().Get()));
        }
    };

    template <uint32_t Slot>
    struct ShaderParameter<ShaderParamType::Sampler, Slot, GfxSamplerHandle> {
        static constexpr ShaderParamType kType = ShaderParamType::Sampler;
        static constexpr uint32_t kSlot = Slot;
        using ValueType = GfxSamplerHandle;

        GfxSamplerHandle value{};

        static GfxBindingLayoutItem makeLayoutItem() {
            return GfxBindingLayoutItem::Sampler(Slot);
        }
        static void addToBindingSet(GfxBindingSetDesc& desc, const GfxSamplerHandle& resolved) {
            desc.addItem(GfxBindingSetItem::Sampler(Slot, resolved.Get()));
        }
    };

    template <uint32_t Slot, typename T>
    struct ShaderParameter<ShaderParamType::PushConstants, Slot, T> {
        static constexpr ShaderParamType kType = ShaderParamType::PushConstants;
        static constexpr uint32_t kSlot = Slot;
        using ValueType = T;

        T value{};

        static GfxBindingLayoutItem makeLayoutItem() {
            return GfxBindingLayoutItem::PushConstants(Slot, sizeof(T));
        }
        static void addToBindingSet(GfxBindingSetDesc& desc, const T& val) {
            (void)desc;
            (void)val;
        }
    };

    template <uint32_t Slot>
    struct ShaderParameter<ShaderParamType::ConstantBuffer, Slot, RenderGraphBufferHandle> {
        static constexpr ShaderParamType kType = ShaderParamType::ConstantBuffer;
        static constexpr uint32_t kSlot = Slot;
        using ValueType = RenderGraphBufferHandle;

        RenderGraphBufferHandle value{};

        static GfxBindingLayoutItem makeLayoutItem() {
            return GfxBindingLayoutItem::VolatileConstantBuffer(Slot);
        }
        static void addToBindingSet(GfxBindingSetDesc& desc, const GfxBufferHandle& resolved) {
            desc.addItem(GfxBindingSetItem::ConstantBuffer(Slot, resolved->getRHIHandle().Get()));
        }
    };

    #define SHADER_PARAMETER(type, slot, ...) \
        _SHADER_PARAM_IMPL(type, slot, __VA_ARGS__)

    #define _SHADER_PARAM_IMPL(type, slot, ...) \
        _SHADER_PARAM_EXPAND(type, slot, __VA_ARGS__)

    #define _SHADER_PARAM_EXPAND(type, slot, ...) \
        _SHADER_PARAM_ ## type(slot, __VA_ARGS__)

    #define _SHADER_PARAM_TextureSRV(slot, name) \
        ShaderParameter<dodoe::ShaderParamType::TextureSRV, slot, dodoe::RenderGraphTextureHandle> name{};

    #define _SHADER_PARAM_RawTextureSRV(slot, name) \
        ShaderParameter<dodoe::ShaderParamType::TextureSRV, slot, dodoe::GfxTextureHandle> name{};

    #define SHADER_PARAMETER_RAWTEX(slot, name) \
        ShaderParameter<dodoe::ShaderParamType::TextureSRV, slot, dodoe::GfxTextureHandle> name{};

    #define _SHADER_PARAM_Sampler(slot, name) \
        ShaderParameter<dodoe::ShaderParamType::Sampler, slot, dodoe::GfxSamplerHandle> name;

    #define _SHADER_PARAM_PushConstants(slot, type, name) \
        ShaderParameter<dodoe::ShaderParamType::PushConstants, slot, type> name{};

    #define _SHADER_PARAM_ConstantBuffer(slot, name) \
        ShaderParameter<dodoe::ShaderParamType::ConstantBuffer, slot, dodoe::RenderGraphBufferHandle> name{};

    #define BEGIN_SHADER_PARAMETER_STRUCT(StructName) \
        struct StructName {

    #define END_SHADER_PARAMETER_STRUCT(...) \
        template <typename Func> \
        void forEachMember(Func&& func) { \
            __VA_ARGS__ \
        } \
    };

    template <typename ShaderParamStruct>
    struct ShaderBindingReflector {

        static GfxBindingLayoutHandle getOrCreateLayout(GfxDevice* device,
                                                         GfxShaderType visibility = GfxShaderType::All) {
            static GfxBindingLayoutHandle s_layout = [device, visibility]() {
                if (!device) {
                    DO_ERROR("ShaderBindingReflector::getOrCreateLayout device is null!");
                    return GfxBindingLayoutHandle{};
                }
                GfxBindingLayoutDesc desc;
                desc.setVisibility(visibility);
                ShaderParamStruct dummy{};
                dummy.forEachMember([&desc](auto& member) {
                    desc.addItem(std::decay_t<decltype(member)>::makeLayoutItem());
                });
                auto layout = device->createBindingLayout(desc);
                if (!layout) {
                    DO_ERROR("ShaderBindingReflector::getOrCreateLayout createBindingLayout failed!");
                }
                return layout;
            }();
            if (!s_layout) {
                DO_ERROR("ShaderBindingReflector::getOrCreateLayout returning null layout!");
            }
            return s_layout;
        }

        template <typename ResolveTexFunc, typename ResolveBufFunc>
        static GfxBindingSetHandle createBindingSet(
            GfxDevice* device,
            const GfxBindingLayoutHandle& layout,
            ShaderParamStruct& params,
            ResolveTexFunc&& resolveTex,
            ResolveBufFunc&& resolveBuf)
        {
            GfxBindingSetDesc desc;
            params.forEachMember([&](auto& member) {
                using MemberT = std::decay_t<decltype(member)>;
                if constexpr (MemberT::kType == ShaderParamType::TextureSRV) {
                    if constexpr (std::is_same_v<typename MemberT::ValueType, RenderGraphTextureHandle>) {
                        MemberT::addToBindingSet(desc, resolveTex(member.value));
                    } else {
                        MemberT::addToBindingSet(desc, member.value);
                    }
                } else if constexpr (MemberT::kType == ShaderParamType::Sampler) {
                    MemberT::addToBindingSet(desc, member.value);
                } else if constexpr (MemberT::kType == ShaderParamType::ConstantBuffer) {
                    MemberT::addToBindingSet(desc, resolveBuf(member.value));
                }
            });
            auto bs = create_ref<GfxBindingSet>();
            // Store info for deferred initialization
            GfxBindingSetHandle result = bs;
            return device->createBindingSet(desc, layout);
        }

        template <typename ResolveTexFunc, typename ResolveBufFunc>
        static GfxBindingSetHandle createBindingSetDeferred(
            DrawCommandList& command_list,
            const GfxBindingLayoutHandle& layout,
            ShaderParamStruct& params,
            ResolveTexFunc&& resolveTex,
            ResolveBufFunc&& resolveBuf)
        {
            GfxBindingSetDesc desc;
            params.forEachMember([&](auto& member) {
                using MemberT = std::decay_t<decltype(member)>;
                if constexpr (MemberT::kType == ShaderParamType::TextureSRV) {
                    if constexpr (std::is_same_v<typename MemberT::ValueType, RenderGraphTextureHandle>) {
                        MemberT::addToBindingSet(desc, resolveTex(member.value));
                    } else {
                        MemberT::addToBindingSet(desc, member.value);
                    }
                } else if constexpr (MemberT::kType == ShaderParamType::Sampler) {
                    MemberT::addToBindingSet(desc, member.value);
                } else if constexpr (MemberT::kType == ShaderParamType::ConstantBuffer) {
                    MemberT::addToBindingSet(desc, resolveBuf(member.value));
                }
            });
            return command_list.createBindingSet(desc, layout);
        }
    };

} // dodoe
