#pragma once

// do@Redlive

#include "dopch.h"
#include "engine/render/image.h"
#include "ui_defaults.h"

namespace engine::core {
    class Context;
}
namespace dodoe {
    class UIElement;
    class UIPanel;
    class UIInteractive;
    class UIDragPreview;
}

namespace dodoe {

class UIManager final {
private:
    engine::core::Context& context_;
    dodoe::Scope<UIPanel> root_element_;
    UIInteractive* hovered_element_{nullptr};
    UIInteractive* pressed_element_{nullptr};
    UIDragPreview* drag_preview_{nullptr};
    std::optional<engine::render::Image> cursor_image_{};
    Vector2f cursor_size_{0.0f, 0.0f};
    Vector2f cursor_hotspot_{0.0f, 0.0f};
    bool hid_system_cursor_{false};
    
public:
    UIManager(engine::core::Context& context, const Vector2f& window_size);

    ~UIManager();

    void addElement(dodoe::Scope<UIElement> element);
    UIPanel* getRootElement() const;
    void clearElements();

    void update(float delta_time, engine::core::Context&);
    void render(engine::core::Context&);

    void beginDragPreview(const engine::render::Image& image,
                          int count,
                          const Vector2f& slot_size,
                          float alpha = 0.6f,
                          std::string_view font_path = DEFAULT_UI_FONT_PATH);
    void updateDragPreview(const Vector2f& screen_pos);
    void endDragPreview();
    bool hasDragPreview() const { return drag_preview_ != nullptr; }
    UIInteractive* findInteractiveAt(const Vector2f& screen_pos) const;

    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;
    UIManager(UIManager&&) = delete;
    UIManager& operator=(UIManager&&) = delete;

private:
    void processMouseHover();
    UIInteractive* findTargetAtMouse() const;
    void updateHovered(UIInteractive* target);
    void clearMouseState();
    void initCursor();
    void renderCursor(engine::core::Context& context);

    bool onMousePressed();
    bool onMouseReleased();

    void registerMouseEvents();
    void unregisterMouseEvents();

};

} // namespace dodoe
