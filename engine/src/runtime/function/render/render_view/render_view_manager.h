// do@Redlive

#pragma once

#include "dopch.h"

#include "render_view_target.h"

namespace dodoe {

    class Window;
    class WindowManager;

    struct RenderViewManagerCreateInfo {
        WindowManager* window_manager{nullptr};
    };

    class DODOE_API RenderViewManager : public Managed<RenderViewManager, RenderViewManagerCreateInfo> {
        friend class Managed<RenderViewManager, RenderViewManagerCreateInfo>;
        DynamicArray<Scope<RenderViewTarget>> m_targets;
        RenderViewTarget* m_active_input_target{nullptr};
        Scope<IndexedCameraProvider> m_default_provider;

    public:
        RenderViewTarget* createViewTarget(const RenderViewTargetCreateInfo& info);
        RenderViewTarget* createDefaultViewTarget(Window* window);
        void destroyViewTarget(RenderViewTarget* target);

        [[nodiscard]] const DynamicArray<Scope<RenderViewTarget>>& getTargets() const { return m_targets; }

        void setActiveInputTarget(RenderViewTarget* target) { m_active_input_target = target; }
        [[nodiscard]] RenderViewTarget* getInputTarget() const;

    private:
        Bool initialize(const RenderViewManagerCreateInfo& info);
        void shutdown();
    };

} // namespace dodoe
