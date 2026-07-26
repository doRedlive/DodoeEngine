#pragma once

// do@Redlive

#include "dopch.h"
#include "ui_interactive.h"
#include "ui_compat.h"

namespace dodoe {

enum class UIButtonVisualState : std::uint8_t {
    Normal = 0,
    Hover,
    Pressed,
    Disabled,
    Count
};

struct UIButtonLabelStyle {
    String text;
    String font_path;
    int font_size{16};
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
    Vector2f offset{0.0f, 0.0f};
};

struct UIButtonLabelOverrides {
    std::optional<Color> color{};
    std::optional<Vector2f> offset{};
};

struct UIButtonSkin {
    std::optional<Image> normal_image{};
    std::optional<Image> hover_image{};
    std::optional<Image> pressed_image{};
    std::optional<Image> disabled_image{};
    std::optional<NineSliceMargins> nine_slice_margins{};

    std::optional<UIButtonLabelStyle> normal_label{};
    std::optional<UIButtonLabelOverrides> hover_label{};
    std::optional<UIButtonLabelOverrides> pressed_label{};
    std::optional<UIButtonLabelOverrides> disabled_label{};

    std::unordered_map<identifier, String> sound_events{};
};

class UIButton final : public UIInteractive {
private:
    enum class TextLayoutMode : std::uint8_t {
        Fixed = 0,
        ScaleToFit
    };
    static std::optional<UIButtonVisualState> fromStateId(identifier state_id);

    std::function<void()> m_click_callback{};
    std::function<void()> m_hover_enter_callback{};
    std::function<void()> m_hover_leave_callback{};

    identifier m_preset_id{entt::null};
    UIButtonVisualState m_current_visual_state{UIButtonVisualState::Normal};

    TextLayoutMode m_text_layout_mode{TextLayoutMode::Fixed};
    Thickness m_text_padding{};

    String m_label_text{};
    Vector2f m_base_text_size{0.0f, 0.0f};
    std::uint64_t m_last_label_layout_revision{0};
    identifier m_label_font_id{entt::null};
    int m_label_font_size{0};

public:
    [[nodiscard]] static Scope<UIButton> Create(Context& context,
                                                 identifier preset_id,
                                                 Vector2f position = {0.0f, 0.0f},
                                                 Vector2f size = {0.0f, 0.0f},
                                                 std::function<void()> click_callback = nullptr,
                                                 std::function<void()> hover_enter_callback = nullptr,
                                                 std::function<void()> hover_leave_callback = nullptr);

    [[nodiscard]] static Scope<UIButton> Create(Context& context,
                                                 std::string_view preset_key,
                                                 Vector2f position = {0.0f, 0.0f},
                                                 Vector2f size = {0.0f, 0.0f},
                                                 std::function<void()> click_callback = nullptr,
                                                 std::function<void()> hover_enter_callback = nullptr,
                                                 std::function<void()> hover_leave_callback = nullptr);

    ~UIButton() override = default;

    void update(float delta_time, Context& context) override;
    void applyStateVisual(identifier state_id) override;

    void clicked() override { if (m_click_callback) m_click_callback(); }
    void hover_enter() override { if (m_hover_enter_callback) m_hover_enter_callback(); }
    void hover_leave() override { if (m_hover_leave_callback) m_hover_leave_callback(); }

    void setLabelText(String text);
    [[nodiscard]] std::string_view getLabelText() const { return m_label_text; }

    void setTextLayoutFixed();
    void setTextLayoutScaleToFit(const Thickness& padding = {});

private:
    void renderSelf(Context& context) override;
    void renderLabel(Context& context, const UIButtonSkin& skin,
                     const Vector2f& position, const Vector2f& size);

    [[nodiscard]] const UIButtonSkin* getPreset() const;

    UIButton(Context& context,
             Vector2f position,
             Vector2f size,
             std::function<void()> click_callback,
             std::function<void()> hover_enter_callback,
             std::function<void()> hover_leave_callback);

    [[nodiscard]] bool initFromPreset(identifier preset_id);

    void refreshBaseTextSize();
    void refreshBaseTextSizeIfNeeded();
};

} // namespace dodoe



