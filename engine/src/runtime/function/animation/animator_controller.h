// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/object/object.h"
#include "runtime/core/object/pptr.h"
#include "anim2d_clip.h"
#include "anim_clip.h"

namespace dodoe {

    enum class AnimatorClipType : UInt8 {
        None,
        Clip2D,
        Clip3D
    };

    struct AnimatorClipRef {
        AnimatorClipType type{AnimatorClipType::None};
        PPtr<Anim2DClip> clip_2d{};
        PPtr<AnimClip> clip_3d{};

        AnimatorClipRef() = default;
        AnimatorClipRef(const PPtr<Anim2DClip>& clip) : type(AnimatorClipType::Clip2D), clip_2d(clip) {}
        AnimatorClipRef(const PPtr<AnimClip>& clip) : type(AnimatorClipType::Clip3D), clip_3d(clip) {}
        explicit AnimatorClipRef(const AnimatorClipType in_type) : type(in_type) {}
    };


    enum class AnimatorParameterType : UInt8 {
        Float,
        Int,
        Bool,
        Trigger
    };

    struct AnimatorParameter {
        String name{};
        AnimatorParameterType type{AnimatorParameterType::Float};
        Float default_float{0.0f};
        Int32 default_int{0};
        Bool default_bool{false};
    };

    enum class AnimatorConditionMode : UInt8 {
        Equals,
        NotEqual,
        Greater,
        Less,
        If,
        IfNot
    };

    struct AnimatorCondition {
        String parameter{};
        AnimatorConditionMode mode{AnimatorConditionMode::Greater};
        Float threshold{0.0f};
    };

    struct AnimatorTransition {
        Size_t from_state{0};
        Size_t to_state{0};
        DynamicArray<AnimatorCondition> conditions{};
        Bool has_exit_time{false};
        Float exit_time{1.0f};
        Float duration{0.0f};
    };

    struct AnimatorState {
        String name{};
        AnimatorClipRef clip{};
        Bool loop{true};
        Float speed{1.0f};
    };

    class DODOE_API AnimatorController : public Object {
        DynamicArray<AnimatorParameter> m_parameters{};
        DynamicArray<AnimatorState> m_states{};
        DynamicArray<AnimatorTransition> m_transitions{};
        Size_t m_default_state{0};
        String m_path{};

    public:
        static constexpr Size_t kInvalidState = static_cast<Size_t>(-1);
        static constexpr UInt32 kLocalId = 0;

        AnimatorController() = default;
        explicit AnimatorController(const ObjectID& id)
            : Object(id) {}

        [[nodiscard]] const char* getObjectTypeName() const override { return "AnimatorController"; }

        [[nodiscard]] Size_t getStateCount() const { return m_states.size(); }
        [[nodiscard]] Size_t getTransitionCount() const { return m_transitions.size(); }
        [[nodiscard]] Size_t getParameterCount() const { return m_parameters.size(); }

        [[nodiscard]] const AnimatorState& getState(Size_t index) const { return m_states[index]; }
        [[nodiscard]] const AnimatorTransition& getTransition(Size_t index) const { return m_transitions[index]; }
        [[nodiscard]] const AnimatorParameter& getParameter(Size_t index) const { return m_parameters[index]; }

        [[nodiscard]] Size_t getDefaultState() const { return m_default_state; }
        void setDefaultState(Size_t index) { m_default_state = index; }

        [[nodiscard]] const String& getPath() const { return m_path; }
        void setPath(const String& path) { m_path = path; }

        [[nodiscard]] Size_t findState(const String& name) const {
            for (Size_t i = 0; i < m_states.size(); ++i) {
                if (m_states[i].name == name) {
                    return i;
                }
            }
            return kInvalidState;
        }

        [[nodiscard]] const AnimatorParameter* findParameter(const String& name) const {
            for (const auto& parameter : m_parameters) {
                if (parameter.name == name) {
                    return &parameter;
                }
            }
            return nullptr;
        }

        Size_t addState(const String& name, const AnimatorClipRef& clip = {}, Bool loop = true) {
            AnimatorState state;
            state.name = name;
            state.clip = clip;
            state.loop = loop;
            m_states.push_back(std::move(state));
            return m_states.size() - 1;
        }

        Size_t addParameter(const AnimatorParameter& parameter) {
            m_parameters.push_back(parameter);
            return m_parameters.size() - 1;
        }

        Size_t addTransition(const Size_t from,
                             const Size_t to,
                             DynamicArray<AnimatorCondition> conditions = {},
                             const Bool has_exit_time = false,
                             const Float exit_time = 1.0f,
                             const Float duration = 0.0f) {
            AnimatorTransition transition;
            transition.from_state = from;
            transition.to_state = to;
            transition.conditions = std::move(conditions);
            transition.has_exit_time = has_exit_time;
            transition.exit_time = exit_time;
            transition.duration = duration;
            m_transitions.push_back(std::move(transition));
            return m_transitions.size() - 1;
        }

        Bool loadFromJson(const String& absolute_path);
        [[nodiscard]] Bool saveToJson(const String& absolute_path) const;

        [[nodiscard]] static AnimatorController* Create(const ObjectID& ref, const String& path);
        static void Shutdown();
    };

} // dodoe
