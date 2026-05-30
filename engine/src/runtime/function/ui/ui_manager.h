#pragma once

// do@Redlive

#include "dopch.h"
#include "ui_compat.h"
#include "ui_defaults.h"

namespace dodoe {
    class UIElement;
    class UIPanel;
    class UIInteractive;
    class UIDragPreview;
}

namespace dodoe {

class UIManager final {
private:
    Context& context_;
    dodoe::Scope<UIPanel> root_element_;
    UIInteractive* hovered_element_{nullptr};
    UIInteractive* pressed_element_{nullptr};
    UIDragPreview* drag_preview_{nullptr};
    std::optional<Image> cursor_image_{};
    Vector2f cursor_size_{0.0f, 0.0f};
    Vector2f cursor_hotspot_{0.0f, 0.0f};
    bool hid_system_cursor_{false};
    bool was_mouse_down_{false};
    
public:
    UIManager(Context& context, const Vector2f& window_size);

    ~UIManager();

    void addElement(dodoe::Scope<UIElement> element);
    UIPanel* getRootElement() const;
    void clearElements();

    void update(float delta_time, Context&);
    void render(Context&);

    void beginDragPreview(const Image& image,
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
    void renderCursor(Context& context);

    bool onMousePressed();
    bool onMouseReleased();

    void registerMouseEvents();
    void unregisterMouseEvents();

};

} // namespace dodoe


