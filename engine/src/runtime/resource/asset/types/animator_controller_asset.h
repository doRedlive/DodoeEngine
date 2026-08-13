// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/asset.h"
#include "runtime/function/animation/animator_controller.h"

namespace dodoe {

    class AnimatorControllerAsset : public Asset {
        DynamicArray<AnimatorParameter> m_parameters{};
        DynamicArray<AnimatorState> m_states{};
        DynamicArray<AnimatorTransition> m_transitions{};
        Size_t m_default_state{0};

    public:
        static constexpr AssetType kStaticType = AssetType::AnimatorController;

        AnimatorControllerAsset() { m_meta.type = AssetType::AnimatorController; }

        [[nodiscard]] Bool loadFromSource(const String& absolute_source_path) override;
        void unloadRuntime() override;
        [[nodiscard]] Bool isReadOnly() const override { return false; }
        [[nodiscard]] Bool saveToSource(const String& absolute_path) const override;

        [[nodiscard]] const DynamicArray<AnimatorParameter>& getParameters() const { return m_parameters; }
        [[nodiscard]] const DynamicArray<AnimatorState>& getStates() const { return m_states; }
        [[nodiscard]] const DynamicArray<AnimatorTransition>& getTransitions() const { return m_transitions; }
        [[nodiscard]] Size_t getDefaultState() const { return m_default_state; }
    };

} // dodoe
