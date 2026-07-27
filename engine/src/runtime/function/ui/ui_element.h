// do@Redlive

#pragma once

#include "dopch.h"

#include "ui_types.h"
#include "runtime/core/utils/util.h"

namespace dodoe {

    class UIElement {
    private:
        identifier m_id{entt::null};
        UIElement* m_parent{nullptr};
        DynamicArray<Scope<UIElement>> m_children;

        Bool m_visible{true};
        Float m_depth{0};
        Vector2f m_anchor_min{0, 0};
        Vector2f m_anchor_max{0, 0};
        Vector2f m_pivot{0, 0};
        Vector2f m_position{0, 0};
        Vector2f m_size{100, 100};
        Thickness m_padding{};
        Thickness m_margin{};

        mutable Bool m_layout_dirty{true};
        mutable Vector2f m_cached_screen_pos{0, 0};
        mutable Vector2f m_cached_layout_size{0, 0};

    public:
        UIElement() = default;
        virtual ~UIElement() = default;

        UIElement(const UIElement&) = delete;
        UIElement& operator=(const UIElement&) = delete;

        [[nodiscard]] UIElement* getParent() const { return m_parent; }
        [[nodiscard]] const DynamicArray<Scope<UIElement>>& getChildren() const { return m_children; }
        void addChild(Scope<UIElement> child);
        Scope<UIElement> removeChild(UIElement* child);
        [[nodiscard]] UIElement* findChildById(identifier id) const;

        [[nodiscard]] identifier getId() const { return m_id; }
        void setId(identifier id) { m_id = id; }

        [[nodiscard]] Bool isVisible() const { return m_visible; }
        void setVisible(Bool visible) { m_visible = visible; }

        [[nodiscard]] Float getDepth() const { return m_depth; }
        void setDepth(Float depth) { m_depth = depth; }

        void setAnchor(Vector2f min, Vector2f max);
        void setPivot(Vector2f pivot);
        void setSize(Vector2f size);
        void setPosition(Vector2f position);
        void setPadding(const Thickness& padding);
        void setMargin(const Thickness& margin);

        [[nodiscard]] Vector2f getAnchorMin() const { return m_anchor_min; }
        [[nodiscard]] Vector2f getAnchorMax() const { return m_anchor_max; }
        [[nodiscard]] Vector2f getPivot() const { return m_pivot; }
        [[nodiscard]] Vector2f getSize() const { return m_size; }
        [[nodiscard]] Vector2f getPosition() const { return m_position; }
        [[nodiscard]] const Thickness& getPadding() const { return m_padding; }
        [[nodiscard]] const Thickness& getMargin() const { return m_margin; }

        [[nodiscard]] Vector2f getScreenPosition() const;
        [[nodiscard]] Vector2f getLayoutSize() const;
        [[nodiscard]] Rect getScreenRect() const;

        virtual void onLayout() {}
        virtual void onCollectRenderData(class UIRenderBatch& batch);
        [[nodiscard]] virtual Bool hitTest(Vector2f localPos) const;

    protected:
        void invalidateLayout(Bool propagate = true);
        void ensureLayout() const;
    };

} // namespace dodoe
