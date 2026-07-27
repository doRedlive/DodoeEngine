// do@Redlive

#pragma once

#include "dopch.h"

#include "ui_widget.h"

namespace dodoe {

    class Texture2D;

    class UILabel : public UIWidget {
    private:
        String m_text{};
        Int m_font_size{16};
        Texture2D* m_font_atlas{nullptr};
        TextAnchor m_alignment{TextAnchor::MiddleCenter};
        Float m_line_spacing{1};

    public:
        void setText(String text) { m_text = std::move(text); }
        [[nodiscard]] const String& getText() const { return m_text; }
        void setFontSize(Int size) { m_font_size = size; }
        [[nodiscard]] Int getFontSize() const { return m_font_size; }
        void setFontAtlas(Texture2D* atlas) { m_font_atlas = atlas; }
        void setTextAlignment(TextAnchor alignment) { m_alignment = alignment; }
        void setLineSpacing(Float spacing) { m_line_spacing = spacing; }

    protected:
        void onCollectRenderData(class UIRenderBatch& batch) override;
        void onLayout() override;
    };

} // namespace dodoe
