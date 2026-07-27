// do@Redlive

#pragma once

#include "dopch.h"

#include "ui_interactive.h"
#include "ui_types.h"

namespace dodoe {

    struct ButtonPreset;
    class Texture2D;
    class UIImage;
    class UILabel;

    struct ButtonVisual {
        Texture2D* texture{nullptr};
        Rect uv_rect{0, 0, 1, 1};
        Color color{1, 1, 1, 1};
    };

    class UIButton : public UIInteractive {
    private:
        ButtonState m_state{ButtonState::Normal};
        UIImage* m_icon{nullptr};
        UILabel* m_label{nullptr};
        identifier m_preset_id{0};
        ButtonVisual m_visuals[4]{};

    public:
        void setPreset(identifier presetId) { m_preset_id = presetId; }
        void setLabel(const String& text);
        [[nodiscard]] String getLabel() const;
        void setStateImage(ButtonState state, Texture2D* texture, Rect uv);
        void setStateColor(ButtonState state, Color color);
        void applyPreset(const ButtonPreset& preset);

    protected:
        void onCollectRenderData(class UIRenderBatch& batch) override;
        void onMouseEnter() override;
        void onMouseExit() override;
        void onMouseDown() override;
        void onMouseUp(Bool isInside) override;

    private:
        void transitionTo(ButtonState state);
        void applyVisuals();
    };

} // namespace dodoe
