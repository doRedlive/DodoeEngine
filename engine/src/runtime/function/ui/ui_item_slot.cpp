#include "ui_item_slot.h"
#include <algorithm>
#include <memory>
#include <string>
#include <glm/geometric.hpp>
#include "state/ui_normal_state.h"

namespace dodoe {

UIItemSlot::UIItemSlot(Context& context, Vector2f position, Vector2f size)
    : UIInteractive(context, position, size) {
    
    auto icon = create_scope<UIImage>(Image{});
    icon->setOrderIndex(1);
    icon->setAnchor({0.1f, 0.1f}, {0.9f, 0.9f});
    icon->setVisible(false);
    icon_image_ = icon.get();
    addChild(std::move(icon));

    auto cd = create_scope<UIPanel>(Vector2f{0.0f, 0.0f},
                                        Vector2f{0.0f, 0.0f},
                                        Color{0.0f, 0.0f, 0.0f, 0.55f});
    cd->setOrderIndex(2);
    cd->setAnchor({0, 0}, {1, 0});
    cd->setVisible(false);
    cooldown_overlay_ = cd.get();
    addChild(std::move(cd));

    auto lbl = create_scope<UILabel>(context.getTextRenderer(), "");
    lbl->setOrderIndex(3);
    lbl->setAnchor({0.0f, 0.0f}, {0.0f, 0.0f});
    lbl->setVisible(false);
    count_label_ = lbl.get();
    addChild(std::move(lbl));

    auto sel = create_scope<UIImage>(Image{});
    sel->setOrderIndex(4);
    sel->setAnchor({0, 0}, {1, 1});
    sel->setVisible(false);
    selection_frame_ = sel.get();
    addChild(std::move(sel));

    disableHoverSound();
    disableClickSound();

    setState(create_scope<UINormalState>(this));
}

void UIItemSlot::setItem(const Image& icon, int count) {
    setSlotItem(entt::null, count, icon);
}

void UIItemSlot::setSlotItem(identifier item_id, int count, const Image& icon) {
    if (count <= 0) {
        slot_item_.reset();
        clearItem();
        return;
    }
    slot_item_ = SlotItem{item_id, count, icon};
    setItemIcon(icon);
    setItemCount(count);
}

void UIItemSlot::setSlotItem(const SlotItem& item) {
    setSlotItem(item.item_id, item.count, item.icon);
}

void UIItemSlot::setItemIcon(const Image& icon) {
    if (icon_image_) {
        icon_image_->setImage(icon);
        icon_image_->setVisible(true);
    }
    if (slot_item_) {
        slot_item_->icon = icon;
    }
}

Vector2f UIItemSlot::getIconLayoutSize() const {
    if (icon_image_) {
        Vector2f size = icon_image_->getLayoutSize();
        if (size.x > 0.0f && size.y > 0.0f) {
            return size;
        }
        return icon_image_->getRequestedSize();
    }
    return {0.0f, 0.0f};
}

void UIItemSlot::setItemCount(int count) {
    if (count_label_) {
        if (count > 1) {
            count_label_->setText(std::to_string(count));
            count_label_->setVisible(true);
        } else {
            count_label_->setVisible(false);
        }
        invalidateLayout();
    }
    if (slot_item_) {
        slot_item_->count = (std::max)(count, 0);
    }
}

void UIItemSlot::clearItem() {
    if (icon_image_) icon_image_->setVisible(false);
    if (count_label_) count_label_->setVisible(false);
    slot_item_.reset();
}

void UIItemSlot::setSelected(bool selected) {
    selected_ = selected;
    if (selection_frame_) {
        selection_frame_->setVisible(selected);
    }
}

void UIItemSlot::setCooldown(float percent) {
    if (cooldown_overlay_) {
        percent = std::clamp(percent, 0.0f, 1.0f);
        cooldown_percent_ = percent;
        if (percent > 0.001f) {
            cooldown_overlay_->setVisible(true);
            cooldown_overlay_->setAnchor({0, 0}, {1, percent});
        } else {
            cooldown_overlay_->setVisible(false);
        }
    }
}

void UIItemSlot::setSelectionImage(const Image& image) {
    if (selection_frame_) {
        selection_frame_->setImage(image);
    }
}

void UIItemSlot::setBackgroundImage(const Image& image) {
    addImage(UI_IMAGE_NORMAL_ID, image);
    setCurrentImage(UI_IMAGE_NORMAL_ID);
}

void UIItemSlot::applyStateVisual(identifier state_id) {
    if (m_images.contains(state_id)) {
        setCurrentImage(state_id);
        return;
    }

    if (state_id != UI_IMAGE_NORMAL_ID && m_images.contains(UI_IMAGE_NORMAL_ID)) {
        setCurrentImage(UI_IMAGE_NORMAL_ID);
    }
}

void UIItemSlot::onLayout() {
    if (count_label_ && count_label_->isVisible()) {
        Vector2f lbl_size = count_label_->getSize();
        Vector2f padding = {2.0f, 2.0f};

        Vector2f pos = getPosition();
        if (icon_image_) {
            const auto icon_bounds = icon_image_->getBounds();
            const auto parent_screen_pos = getScreenPosition();
            Vector2f icon_local_pos = icon_bounds.pos - parent_screen_pos;

            pos.x = icon_local_pos.x + icon_bounds.size.x - lbl_size.x - padding.x;
            pos.y = icon_local_pos.y + icon_bounds.size.y - lbl_size.y - padding.y;
        } else {
            Vector2f my_size = getSize();
            pos.x = my_size.x - lbl_size.x - padding.x;
            pos.y = my_size.y - lbl_size.y - padding.y;
        }
        
        if (glm::distance(count_label_->getPosition(), pos) > 0.001f) {
            count_label_->setPosition(pos);
        }
    }
}

} // namespace dodoe


