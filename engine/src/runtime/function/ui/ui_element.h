#pragma once

// do@Redlive

#include "dopch.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    class Context;

    class UIInteractive;

    struct Thickness {
        float left{0.0f};
        float top{0.0f};
        float right{0.0f};
        float bottom{0.0f};

        [[nodiscard]] Vector2f horizontal() const { return {left, right}; }
        [[nodiscard]] Vector2f vertical() const { return {top, bottom}; }
        [[nodiscard]] float width() const { return left + right; }
        [[nodiscard]] float height() const { return top + bottom; }
    };

    class UIElement {
    protected:
        Vector2f m_position{};
        Vector2f m_size{};
        bool m_visible = true;
        bool m_need_remove = false;
        int m_order_index = 0;
        identifier m_id = entt::null;

        UIElement* m_parent = nullptr;
        std::vector<Scope<UIElement>> m_children;

        Vector2f m_anchor_min{0.0f, 0.0f};
        Vector2f m_anchor_max{0.0f, 0.0f};
        Vector2f m_pivot{0.0f, 0.0f};
        Thickness m_padding{};
        Thickness m_margin{};

        mutable bool m_layout_dirty{true};
        mutable Vector2f m_layout_position{0.0f, 0.0f};
        mutable Vector2f m_layout_size{0.0f, 0.0f};

    public:
        explicit UIElement(Vector2f position = {0.0f, 0.0f}, Vector2f size = {0.0f, 0.0f});
        virtual ~UIElement() = default;

        virtual void update(float delta_time, Context& context);
        virtual void render(Context& context);

        void addChild(Scope<UIElement> child, int order_index = -1);
        Scope<UIElement> removeChild(UIElement* child_ptr);
        Scope<UIElement> removeChildById(identifier id);
        void removeAllChildren();

        [[nodiscard]] Vector2f getSize() const;
        [[nodiscard]] const Vector2f& getRequestedSize() const { return m_size; }
        [[nodiscard]] const Vector2f& getPosition() const { return m_position; }
        [[nodiscard]] bool isVisible() const { return m_visible; }
        [[nodiscard]] bool isNeedRemove() const { return m_need_remove; }
        [[nodiscard]] int getOrderIndex() const { return m_order_index; }
        [[nodiscard]] UIElement* getParent() const { return m_parent; }
        [[nodiscard]] const std::vector<Scope<UIElement>>& getChildren() const { return m_children; }
        [[nodiscard]] UIElement* getChildById(identifier id) const;
        [[nodiscard]] identifier getId() const { return m_id; }
        [[nodiscard]] Rect getBounds() const;
        [[nodiscard]] Vector2f getScreenPosition() const;
        [[nodiscard]] Vector2f getLayoutSize() const;
        [[nodiscard]] Vector2f getLayoutPosition() const { return getScreenPosition(); }
        [[nodiscard]] Rect getContentBounds() const;
        [[nodiscard]] const Thickness& getPadding() const { return m_padding; }
        [[nodiscard]] const Thickness& getMargin() const { return m_margin; }
        [[nodiscard]] Vector2f getAnchorMin() const { return m_anchor_min; }
        [[nodiscard]] Vector2f getAnchorMax() const { return m_anchor_max; }
        [[nodiscard]] Vector2f getPivot() const { return m_pivot; }

        void setSize(Vector2f size) { setSizeInternal(std::move(size)); }
        void setVisible(bool visible) { m_visible = visible; }
        void setParent(UIElement* parent) { setParentInternal(parent); }
        void setPosition(Vector2f position) { m_position = std::move(position); invalidateLayout(); }
        void setNeedRemove(bool need_remove) { m_need_remove = need_remove; }
        void setOrderIndex(int order_index);
        void setId(identifier id) { m_id = id; }
        void setAnchor(Vector2f anchor_min, Vector2f anchor_max);
        void setPivot(Vector2f pivot);
        void setPadding(const Thickness& padding);
        void setMargin(const Thickness& margin);

        void sortChildrenByOrderIndex();
        bool isPointInside(const Vector2f& point) const;

        UIInteractive* findInteractiveAt(const Vector2f& point);
        const UIInteractive* findInteractiveAt(const Vector2f& point) const;

        UIElement(const UIElement&) = delete;
        UIElement& operator=(const UIElement&) = delete;
        UIElement(UIElement&&) = delete;
        UIElement& operator=(UIElement&&) = delete;

    protected:
        virtual void renderSelf(Context& context);
        virtual void onLayout() {}

        void invalidateLayout(bool propagate = true);
        void ensureLayout() const;
        void setParentInternal(UIElement* parent);
        void setSizeInternal(Vector2f size);
    };

} // dodoe

