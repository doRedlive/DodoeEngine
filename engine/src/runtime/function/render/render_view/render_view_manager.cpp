// do@Redlive

#include "render_view_manager.h"

#include "runtime/function/window/window.h"

namespace dodoe {

    Bool RenderViewManager::initialize(const RenderViewManagerCreateInfo& info) {
        m_default_provider = create_scope<IndexedCameraProvider>(0);
        auto window = info.window_manager->getWindow();
        createDefaultViewTarget(window);
        return true;
    }

    void RenderViewManager::shutdown() {
        while (!m_targets.empty()) {
            destroyViewTarget(m_targets.back().get());
        }
        m_default_provider.reset();
    }

    RenderViewTarget* RenderViewManager::createViewTarget(const RenderViewTargetCreateInfo& info) {
        auto target = RenderViewTarget::Create(info);
        if (!target) return nullptr;
        auto* ptr = target.get();
        m_targets.push_back(std::move(target));
        if (!m_active_input_target) {
            m_active_input_target = ptr;
        }
        return ptr;
    }

    RenderViewTarget* RenderViewManager::createDefaultViewTarget(Window* window) {
        RenderViewTargetCreateInfo info;
        info.logical = Vector2f(640.0f, 360.0f);
        info.pixel   = window->getPixelSize();
        info.window  = Vector2i(window->getWidth(), window->getHeight());
        info.camera  = m_default_provider.get();
        return createViewTarget(info);
    }

    void RenderViewManager::destroyViewTarget(RenderViewTarget* target) {
        if (!target) return;
        if (m_active_input_target == target) {
            m_active_input_target = nullptr;
        }
        auto it = std::find_if(m_targets.begin(), m_targets.end(),
                               [target](const auto& e) { return e.get() == target; });
        if (it != m_targets.end()) {
            auto scope = extract_scope(m_targets, it);
            RenderViewTarget::Destroy(scope);
        }
    }

    RenderViewTarget* RenderViewManager::getInputTarget() const {
        if (m_active_input_target) return m_active_input_target;
        return m_targets.empty() ? nullptr : m_targets[0].get();
    }

} // namespace dodoe
