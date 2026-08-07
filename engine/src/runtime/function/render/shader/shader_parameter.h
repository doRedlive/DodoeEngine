// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/shader/shader_reflection.h"

namespace dodoe {

    struct RenderGraphTextureHandle;
    struct RenderGraphBufferHandle;

    enum class ShaderParameterSet : UInt32 {
        Global = 0,
        View = 1,
        Pass = 2,
        Material = 3,
        Primitive = 4,
        Bindless = 5,
        None = 0xFF,
    };

    namespace shader_bindings {
        constexpr UInt32 kGlobalBindingConstants = 0;
        constexpr UInt32 kViewBindingConstants = 0;
        constexpr UInt32 kPassBindingConstants = 0;
        constexpr UInt32 kPassBindingInput0 = 1;
        constexpr UInt32 kPassBindingInput1 = 2;
        constexpr UInt32 kPassBindingInput2 = 3;
        constexpr UInt32 kPassBindingInput3 = 4;
        constexpr UInt32 kPassBindingInput4 = 5;
        constexpr UInt32 kPassBindingInput5 = 6;
        constexpr UInt32 kPassBindingInput6 = 7;
        constexpr UInt32 kPassBindingInput7 = 8;
        constexpr UInt32 kPassBindingSampler = 9;
        constexpr UInt32 kMaterialBindingConstants = 0;
        constexpr UInt32 kMaterialBindingSampler = 1;
        constexpr UInt32 kMaterialBindingBaseColor = 2;
        constexpr UInt32 kMaterialBindingMetallicRough = 3;
        constexpr UInt32 kMaterialBindingNormal = 4;
        constexpr UInt32 kMaterialBindingEmissive = 5;
        constexpr UInt32 kPrimitiveBindingConstants = 0;
        constexpr UInt32 kBindlessBindingTextures = 0;
    }

    enum class ShaderParamType : uint8_t {
        TextureSRV,
        Sampler,
        PushConstants,
        ConstantBuffer,
    };

    template <ShaderParamType Type>
    struct ShaderParamDefaultValueType;
    template <>
    struct ShaderParamDefaultValueType<ShaderParamType::TextureSRV> { using type = RenderGraphTextureHandle; };
    template <>
    struct ShaderParamDefaultValueType<ShaderParamType::Sampler> { using type = GfxSamplerHandle; };
    template <>
    struct ShaderParamDefaultValueType<ShaderParamType::ConstantBuffer> { using type = RenderGraphBufferHandle; };
    template <>
    struct ShaderParamDefaultValueType<ShaderParamType::PushConstants> { using type = void; };

    template <ShaderParamType Type, uint32_t Set, uint32_t Binding, typename ValueT = void>
    struct ShaderParameter;

    template <uint32_t Set, uint32_t Binding>
    struct ShaderParameter<ShaderParamType::TextureSRV, Set, Binding, RenderGraphTextureHandle> {
        static constexpr ShaderParamType kType = ShaderParamType::TextureSRV;
        static constexpr uint32_t kSet = Set;
        static constexpr uint32_t kBinding = Binding;
        using ValueType = RenderGraphTextureHandle;

        ValueType value{};

        static GfxBindingLayoutItem makeLayoutItem() {
            return GfxBindingLayoutItem::Texture_SRV(Binding);
        }
        static void addToBindingSet(GfxBindingSetDesc& desc, const GfxTextureHandle& resolved) {
            desc.addItem(GfxBindingSetItem::Texture_SRV(Binding, resolved->getRHIHandle().Get()));
        }
    };

    template <uint32_t Set, uint32_t Binding>
    struct ShaderParameter<ShaderParamType::TextureSRV, Set, Binding, GfxTextureHandle> {
        static constexpr ShaderParamType kType = ShaderParamType::TextureSRV;
        static constexpr uint32_t kSet = Set;
        static constexpr uint32_t kBinding = Binding;
        using ValueType = GfxTextureHandle;

        ValueType value{};

        static GfxBindingLayoutItem makeLayoutItem() {
            return GfxBindingLayoutItem::Texture_SRV(Binding);
        }
        static void addToBindingSet(GfxBindingSetDesc& desc, const GfxTextureHandle& resolved) {
            desc.addItem(GfxBindingSetItem::Texture_SRV(Binding, resolved->getRHIHandle().Get()));
        }
    };

    template <uint32_t Set, uint32_t Binding>
    struct ShaderParameter<ShaderParamType::Sampler, Set, Binding, GfxSamplerHandle> {
        static constexpr ShaderParamType kType = ShaderParamType::Sampler;
        static constexpr uint32_t kSet = Set;
        static constexpr uint32_t kBinding = Binding;
        using ValueType = GfxSamplerHandle;

        ValueType value{};

        static GfxBindingLayoutItem makeLayoutItem() {
            return GfxBindingLayoutItem::Sampler(Binding);
        }
        static void addToBindingSet(GfxBindingSetDesc& desc, const GfxSamplerHandle& resolved) {
            desc.addItem(GfxBindingSetItem::Sampler(Binding, resolved.Get()));
        }
    };

    template <uint32_t Set, uint32_t Binding, typename T>
    struct ShaderParameter<ShaderParamType::PushConstants, Set, Binding, T> {
        static constexpr ShaderParamType kType = ShaderParamType::PushConstants;
        static constexpr uint32_t kSet = Set;
        static constexpr uint32_t kBinding = Binding;
        using ValueType = T;

        T value{};

        static GfxBindingLayoutItem makeLayoutItem() {
            return GfxBindingLayoutItem::PushConstants(Binding, sizeof(T));
        }
        static void addToBindingSet(GfxBindingSetDesc& desc, const T& val) {
            (void)desc;
            (void)val;
        }
    };

    template <uint32_t Set, uint32_t Binding>
    struct ShaderParameter<ShaderParamType::ConstantBuffer, Set, Binding, RenderGraphBufferHandle> {
        static constexpr ShaderParamType kType = ShaderParamType::ConstantBuffer;
        static constexpr uint32_t kSet = Set;
        static constexpr uint32_t kBinding = Binding;
        using ValueType = RenderGraphBufferHandle;

        ValueType value{};

        static GfxBindingLayoutItem makeLayoutItem() {
            return GfxBindingLayoutItem::VolatileConstantBuffer(Binding);
        }
        static void addToBindingSet(GfxBindingSetDesc& desc, const GfxBufferHandle& resolved) {
            desc.addItem(GfxBindingSetItem::ConstantBuffer(Binding, resolved->getRHIHandle().Get()));
        }
    };

    template <uint32_t Set, uint32_t Binding>
    struct ShaderParameter<ShaderParamType::ConstantBuffer, Set, Binding, GfxBufferHandle> {
        static constexpr ShaderParamType kType = ShaderParamType::ConstantBuffer;
        static constexpr uint32_t kSet = Set;
        static constexpr uint32_t kBinding = Binding;
        using ValueType = GfxBufferHandle;

        ValueType value{};

        static GfxBindingLayoutItem makeLayoutItem() {
            return GfxBindingLayoutItem::ConstantBuffer(Binding);
        }
        static void addToBindingSet(GfxBindingSetDesc& desc, const GfxBufferHandle& resolved) {
            desc.addItem(GfxBindingSetItem::ConstantBuffer(Binding, resolved->getRHIHandle().Get()));
        }
    };

    template <ShaderParamType Type, uint32_t Set, uint32_t Binding>
    using DefaultShaderParameter = ShaderParameter<Type, Set, Binding, typename ShaderParamDefaultValueType<Type>::type>;

    #define BEGIN_SHADER_PARAMETER_STRUCT(StructName, Set) \
        struct StructName { \
            static constexpr dodoe::ShaderParameterSet kSet = dodoe::ShaderParameterSet::Set;

    #define SHADER_PARAMETER(type, binding, name) \
        dodoe::DefaultShaderParameter<dodoe::ShaderParamType::type, (uint32_t)kSet, binding> name{};

    #define SHADER_PARAMETER_SET(type, set, binding, name) \
        dodoe::DefaultShaderParameter<dodoe::ShaderParamType::type, (uint32_t)dodoe::ShaderParameterSet::set, binding> name{};

    #define SHADER_PARAMETER_RAWTEX(binding, name) \
        dodoe::ShaderParameter<dodoe::ShaderParamType::TextureSRV, (uint32_t)kSet, binding, dodoe::GfxTextureHandle> name{};

    #define SHADER_PARAMETER_RAWCB(binding, name) \
        dodoe::ShaderParameter<dodoe::ShaderParamType::ConstantBuffer, (uint32_t)kSet, binding, dodoe::GfxBufferHandle> name{};

    #define SHADER_PARAMETER_PUSH_CONSTANTS(binding, type, name) \
        dodoe::ShaderParameter<dodoe::ShaderParamType::PushConstants, (uint32_t)kSet, binding, type> name{};

    #define SHADER_PARAMETER_PUSH_CONSTANTS_SET(set, binding, type, name) \
        dodoe::ShaderParameter<dodoe::ShaderParamType::PushConstants, (uint32_t)dodoe::ShaderParameterSet::set, binding, type> name{};

    #define END_SHADER_PARAMETER_STRUCT(...) \
        template <typename Func> \
        void forEachMember(Func&& func) { \
            __VA_ARGS__ \
        } \
    };

    template <typename ShaderParamStruct>
    struct ShaderBindingReflector {

        static DynamicArray<GfxBindingLayoutHandle> getOrCreateLayouts(GfxShaderType visibility = GfxShaderType::All) {
            static DynamicArray<GfxBindingLayoutHandle> s_layouts = [visibility]() {
                DynamicArray<GfxBindingLayoutHandle> layouts;
                ShaderParamStruct dummy{};

                StaticArray<GfxBindingLayoutDesc, 8> set_descs{};
                GfxBindingLayoutDesc push_desc;
                push_desc.setVisibility(visibility);
                push_desc.setRegisterSpaceIsDescriptorSet(true);

                dummy.forEachMember([&](auto& member) {
                    using MemberT = std::decay_t<decltype(member)>;
                    if constexpr (MemberT::kType == ShaderParamType::PushConstants) {
                        push_desc.addItem(MemberT::makeLayoutItem());
                    } else {
                        GfxBindingLayoutDesc& desc = set_descs[MemberT::kSet];
                        desc.setVisibility(visibility);
                        desc.setRegisterSpaceIsDescriptorSet(true);
                        desc.setRegisterSpace(MemberT::kSet);
                        desc.addItem(MemberT::makeLayoutItem());
                    }
                });

                for (UInt32 set = 0; set < set_descs.size(); ++set) {
                    if (!set_descs[set].bindings.empty()) {
                        auto layout = GDrawCommandList.createBindingLayout(set_descs[set]);
                        if (!layout) {
                            DO_ERROR("ShaderBindingReflector::getOrCreateLayouts createBindingLayout failed for set {}", set);
                        }
                        layouts.push_back(layout);
                    }
                }
                if (!push_desc.bindings.empty()) {
                    auto layout = GDrawCommandList.createBindingLayout(push_desc);
                    if (!layout) {
                        DO_ERROR("ShaderBindingReflector::getOrCreateLayouts createBindingLayout failed for push constants");
                    }
                    layouts.push_back(layout);
                }
                return layouts;
            }();
            return s_layouts;
        }

        template <typename ResolveTexFunc, typename ResolveBufFunc>
        static DynamicArray<GfxBindingSetHandle> createBindingSets(
            DrawCommandList& command_list,
            const DynamicArray<GfxBindingLayoutHandle>& layouts,
            ShaderParamStruct& params,
            ResolveTexFunc&& resolveTex,
            ResolveBufFunc&& resolveBuf)
        {
            DynamicArray<GfxBindingSetHandle> result;

            StaticArray<GfxBindingSetDesc, 8> set_descs{};
            Bool has_push{false};
            params.forEachMember([&](auto& member) {
                using MemberT = std::decay_t<decltype(member)>;
                if constexpr (MemberT::kType == ShaderParamType::PushConstants) {
                    has_push = true;
                } else if constexpr (MemberT::kType == ShaderParamType::TextureSRV) {
                    if constexpr (std::is_same_v<typename MemberT::ValueType, RenderGraphTextureHandle>) {
                        MemberT::addToBindingSet(set_descs[MemberT::kSet], resolveTex(member.value));
                    } else {
                        MemberT::addToBindingSet(set_descs[MemberT::kSet], member.value);
                    }
                } else if constexpr (MemberT::kType == ShaderParamType::Sampler) {
                    MemberT::addToBindingSet(set_descs[MemberT::kSet], member.value);
                } else if constexpr (MemberT::kType == ShaderParamType::ConstantBuffer) {
                    if constexpr (std::is_same_v<typename MemberT::ValueType, RenderGraphBufferHandle>) {
                        MemberT::addToBindingSet(set_descs[MemberT::kSet], resolveBuf(member.value));
                    } else {
                        MemberT::addToBindingSet(set_descs[MemberT::kSet], member.value);
                    }
                }
            });

            UInt32 layout_index = 0;
            for (UInt32 set = 0; set < set_descs.size() && layout_index < layouts.size(); ++set) {
                if (set_descs[set].bindings.empty()) {
                    continue;
                }
                auto bs = command_list.createBindingSet(set_descs[set], layouts[layout_index]);
                if (!bs) {
                    DO_ERROR("ShaderBindingReflector::createBindingSets createBindingSet failed for set {}", set);
                }
                result.push_back(bs);
                ++layout_index;
            }
            if (has_push && layout_index < layouts.size()) {
                ++layout_index;
            }
            return result;
        }
    };

    struct ShaderParameterLayout {
        UInt32 set{0};
        UInt32 binding{0};
        ShaderResourceKind kind{ShaderResourceKind::TextureSRV};
        UInt32 array_size{1};
        UInt32 byte_size{0};
        String name{};
        GfxShaderType stage{GfxShaderType::All};
    };

    struct ShaderParameterBindingSet {
        GfxBindingLayoutHandle layout{};
        GfxBindingSetHandle binding_set{};
    };

    class ShaderParameterBinder {
    public:
        static constexpr UInt32 kShaderParameterSetCount = 6;

        void bind(GfxGraphicsState& graphics_state,
                  const StaticArray<GfxBindingSetHandle, kShaderParameterSetCount>& binding_sets) const {
            for (UInt32 set = 0; set < kShaderParameterSetCount; ++set) {
                const auto& binding_set = binding_sets[set];
                if (binding_set && binding_set->isRHIReady()) {
                    graphics_state.addBindingSet(binding_set->getRHIHandle());
                }
            }
        }
    };

} // dodoe
