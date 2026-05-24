#include "ui_manager.h"
#include "ui_panel.h"
#include "ui_element.h"
#include "ui_interactive.h"
#include "ui_imgui_utils.h"
#include "ui_preset_manager.h"
#include "engine/core/context.h"
#include "engine/input/input_manager.h"
#include "engine/resource/resource_manager.h"
#include "ui_drag_preview.h"
#include <entt/core/hashed_string.hpp>
#include <SDL3/SDL.h>

using namespace entt::literals;

namespace dodoe {

namespace {
int g_hidden_system_cursor_count = 0;
} // namespace

UIManager::~UIManager()
{
    unregisterMouseEvents();
    if (hid_system_cursor_) {
        hid_system_cursor_ = false;
        if (g_hidden_system_cursor_count > 0) {
            --g_hidden_system_cursor_count;
        }

        if (g_hidden_system_cursor_count == 0) {
            if (!SDL_ShowCursor()) {
                DO_WARN("UIManager: SDL_ShowCursor failed: {}", SDL_GetError());
            }
        }
    }
    drag_preview_ = nullptr;
}

UIManager::UIManager(engine::core::Context& context, const Vector2f& window_size) : context_(context)
{
    root_element_ = create_scope<UIPanel>(Vector2f{0.0f, 0.0f}, window_size);
    registerMouseEvents();
    initCursor();
    DO_TRACE("UIManager initialized root panel.");
}

void UIManager::addElement(dodoe::Scope<UIElement> element) {
    if (root_element_) {
        root_element_->addChild(std::move(element));
    } else {
        DO_ERROR("UIManager failed to add element: root_element_ is null.");
    }
}

void UIManager::clearElements() {
    if (root_element_) {
        clearMouseState();
        root_element_->removeAllChildren();
        drag_preview_ = nullptr;
        DO_TRACE("UIManager cleared all UI elements.");
    }
}

void UIManager::update(float delta_time, engine::core::Context& context) {
    processMouseHover();

    if (root_element_ && root_element_->isVisible()) {
        root_element_->update(delta_time, context);
    }
}

void UIManager::render(engine::core::Context& context) {
    if (root_element_ && root_element_->isVisible()) {
        root_element_->render(context);
    }
    renderCursor(context);
}

void UIManager::beginDragPreview(const engine::render::Image& image,
                                 int count,
                                 const Vector2f& slot_size,
                                 float alpha,
                                 std::string_view font_path) {
    if (!root_element_) {
        DO_WARN("UIManager::beginDragPreview: root element is null.");
        return;
    }

    if (!drag_preview_) {
        auto preview = create_scope<UIDragPreview>(context_, font_path, DEFAULT_UI_FONT_SIZE_PX, slot_size);
        preview->setAlpha(alpha);
        drag_preview_ = preview.get();
        root_element_->addChild(std::move(preview));
    }

    drag_preview_->setFontPath(font_path);
    drag_preview_->setAlpha(alpha);
    drag_preview_->setContent(image, count, slot_size);
    drag_preview_->setVisible(true);
}

void UIManager::updateDragPreview(const Vector2f& screen_pos) {
    if (!drag_preview_ || !root_element_) {
        return;
    }
    auto parent_content = root_element_->getContentBounds();
    const Vector2f local_pos = screen_pos - parent_content.pos;
    drag_preview_->setPosition(local_pos);
}

void UIManager::endDragPreview() {
    if (drag_preview_) {
        drag_preview_->setVisible(false);
    }
}

UIInteractive* UIManager::findInteractiveAt(const Vector2f& screen_pos) const {
    if (!root_element_ || !root_element_->isVisible()) {
        return nullptr;
    }
    return root_element_->findInteractiveAt(screen_pos);
}

UIPanel* UIManager::getRootElement() const {
    return root_element_.get();
}

void UIManager::processMouseHover() {
    if (!root_element_) {
        clearMouseState();
        return;
    }

    if (!root_element_->isVisible()) {
        clearMouseState();
        return;
    }

    UIInteractive* target = findTargetAtMouse();
    updateHovered(target);
}

UIInteractive* UIManager::findTargetAtMouse() const {
    if (!root_element_ || !root_element_->isVisible()) {
        return nullptr;
    }

    auto& input_manager = context_.getInputManager();
    auto mouse_pos = input_manager.getLogicalMousePosition();
    return root_element_->findInteractiveAt(mouse_pos);
}

bool UIManager::onMousePressed() {
    if (!root_element_ || !root_element_->isVisible()) {
        return false;
    }

    UIInteractive* target = findTargetAtMouse();
    if (!target) {
        pressed_element_ = nullptr;
        return false;
    }

    pressed_element_ = target;
    pressed_element_->mousePressed();
    return true;
}

bool UIManager::onMouseReleased() {
    if (!root_element_ || !root_element_->isVisible()) {
        return false;
    }

    UIInteractive* target = findTargetAtMouse();
    if (pressed_element_) {
        auto* pressed = pressed_element_;
        pressed_element_ = nullptr;
        const bool is_inside = (pressed == target);
        pressed->mouseReleased(is_inside);
        return true;
    }

    return false;
}

void UIManager::updateHovered(UIInteractive* target) {
    if (target == hovered_element_) {
        return;
    }

    if (hovered_element_) {
        hovered_element_->mouseExit();
    }

    hovered_element_ = target;
    if (hovered_element_) {
        hovered_element_->mouseEnter();
    }
}

void UIManager::clearMouseState() {
    if (hovered_element_) {
        hovered_element_->mouseExit();
        hovered_element_ = nullptr;
    }
    pressed_element_ = nullptr;
}

void UIManager::initCursor() {
    constexpr entt::hashed_string CURSOR_PRESET_ID{"cursor"};
    hid_system_cursor_ = false;

    auto& resource_manager = context_.getResourceManager();
    const auto* preset = resource_manager.getUIPresetManager().getImagePreset(CURSOR_PRESET_ID);
    if (!preset) {
        DO_WARN("UIManager: missing cursor preset '{}'.", CURSOR_PRESET_ID.data());
        cursor_image_.reset();
        cursor_size_ = {0.0f, 0.0f};
        cursor_hotspot_ = {0.0f, 0.0f};
        return;
    }

    cursor_image_ = *preset;
    cursor_size_ = preset->getSourceRect().size;
    cursor_hotspot_ = {0.0f, 0.0f};

    if (cursor_size_.x <= 0.0f || cursor_size_.y <= 0.0f) {
        cursor_size_ = resource_manager.getTextureSize(preset->getTextureId(), preset->getTexturePath());
    }

    if (cursor_size_.x <= 0.0f || cursor_size_.y <= 0.0f) {
        DO_WARN("UIManager: cursor preset '{}' has invalid size.", CURSOR_PRESET_ID.data());
        cursor_image_.reset();
        return;
    }

    if (SDL_HideCursor()) {
        hid_system_cursor_ = true;
        ++g_hidden_system_cursor_count;
    } else {
        DO_WARN("UIManager: SDL_HideCursor failed: {}", SDL_GetError());
    }
}

void UIManager::renderCursor(engine::core::Context& context) {
    if (!cursor_image_) {
        return;
    }
    if (cursor_size_.x <= 0.0f || cursor_size_.y <= 0.0f) {
        return;
    }

    const Vector2f mouse_pos = context.getInputManager().getLogicalMousePosition();
    const Vector2f draw_pos = mouse_pos - cursor_hotspot_;
    ui::drawImageForeground(*cursor_image_, draw_pos, cursor_size_);
}

void UIManager::registerMouseEvents() {
    auto& input_manager = context_.getInputManager();
    input_manager.onAction("mouse_left"_hs, engine::input::ActionState::PRESSED).connect<&UIManager::onMousePressed>(this);
    input_manager.onAction("mouse_left"_hs, engine::input::ActionState::RELEASED).connect<&UIManager::onMouseReleased>(this);
}

void UIManager::unregisterMouseEvents() {
    auto& input_manager = context_.getInputManager();
    input_manager.onAction("mouse_left"_hs, engine::input::ActionState::PRESSED)
        .disconnect<&UIManager::onMousePressed>(this);
    input_manager.onAction("mouse_left"_hs, engine::input::ActionState::RELEASED)
        .disconnect<&UIManager::onMouseReleased>(this);
}

} // namespace dodoe 
