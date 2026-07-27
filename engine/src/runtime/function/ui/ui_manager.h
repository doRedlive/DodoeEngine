// do@Redlive

#pragma once

#include "dopch.h"

#include "ui_element.h"
#include "ui_input_router.h"
#include "ui_render_batch.h"
#include "ui_preset_manager.h"

namespace dodoe {

    class WindowManager;

    struct UIManagerCreateInfo {
        WindowManager* window_manager{nullptr};
    };

    class UIManager : public Managed<UIManager, UIManagerCreateInfo> {
    private:
        friend class Managed<UIManager, UIManagerCreateInfo>;

        WindowManager* m_window_manager{nullptr};
        Scope<UIElement> m_root{nullptr};
        Scope<UIInputRouter> m_input_router{nullptr};
        Scope<UIRenderBatch> m_render_batch{nullptr};
        Scope<UIPresetManager> m_preset_manager{nullptr};

    public:
        Bool initialize(const UIManagerCreateInfo& info);
        void shutdown();

        void update(Float deltaTime);

        void setRoot(Scope<UIElement> root);
        [[nodiscard]] UIElement* getRoot() const { return m_root.get(); }
        [[nodiscard]] UIInputRouter* getInputRouter() const { return m_input_router.get(); }
        [[nodiscard]] UIPresetManager* getPresetManager() const { return m_preset_manager.get(); }

        Bool loadLayout(const String& filePath);
        void unloadLayout();
        void clearAll();
        [[nodiscard]] UIElement* findElementById(const String& id) const;

        [[nodiscard]] UIElement* createElement(const String& type, const String& id, const String& parentId);
        Bool removeElement(const String& id);

        void onMouseMove(Vector2f pos);
        void onMouseDown(Vector2f pos);
        void onMouseUp(Vector2f pos);
        void onScroll(Vector2f pos, Float delta);
    };

} // namespace dodoe
