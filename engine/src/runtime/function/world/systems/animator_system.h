// do@Redlive

#pragma once

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"

namespace dodoe {

    class AnimatorController;
    class AnimClip;
    struct AnimatorParameter;
    struct AnimatorCondition;
    struct AnimClipEvent;

    class AnimatorSystem : public System {
    public:
        ~AnimatorSystem() override;

        [[nodiscard]] SystemAccess getAccess() const override;

        void update(Registry& reg, float dt) override;

    private:
        [[nodiscard]] static Float parameterDefault(const AnimatorParameter& parameter);
        [[nodiscard]] static Bool evaluateCondition(AnimatorComponent& animator,
                                                    const AnimatorController& controller,
                                                    const AnimatorCondition& condition);
        static void fireClipEvents(AnimatorComponent& animator,
                                   const DynamicArray<AnimClipEvent>& events,
                                   Float total_ms);
        static void evaluateTransitions(AnimatorComponent& animator,
                                        const AnimatorController& controller,
                                        Float total_ms);
        static void applyPlayRequest(AnimatorComponent& animator, const String& state_name);
        [[nodiscard]] static const AnimClip* resolveClipByPath(const String& path_with_clip);
    };

} // dodoe
