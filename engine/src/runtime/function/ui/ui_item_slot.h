#pragma once
#include "dopch.h"
#include "ui_interactive.h"
#include "ui_image.h"
#include "ui_label.h"
#include "ui_panel.h"

namespace dodoe {

struct SlotItem {
    identifier item_id{entt::null};
    int count{0};
    Image icon{};
};

class UIItemSlot final : public UIInteractive {
private:
    UIImage* icon_image_{nullptr};
    UILabel* count_label_{nullptr};
    UIPanel* cooldown_overlay_{nullptr};
    UIImage* selection_frame_{nullptr};

    bool selected_{false};
    float cooldown_percent_{0.0f};
    std::optional<SlotItem> slot_item_{};

public:
    UIItemSlot(Context& context,
               Vector2f position = {0.0f, 0.0f},
               Vector2f size = {0.0f, 0.0f});

    void setItem(const Image& icon, int count = 1);
    void setSlotItem(identifier item_id, int count, const Image& icon);
    void setSlotItem(const SlotItem& item);
    std::optional<SlotItem> getSlotItem() const { return slot_item_; }
    void clearSlotItem() { clearItem(); }
    void setItemIcon(const Image& icon);
    Vector2f getIconLayoutSize() const;
    void setItemCount(int count);
    void clearItem();

    void setSelected(bool selected);
    bool isSelected() const { return selected_; }
    
    void setCooldown(float percent);
    float getCooldown() const { return cooldown_percent_; }

    void setSelectionImage(const Image& image);
    void setBackgroundImage(const Image& image);

    void applyStateVisual(identifier state_id) override;
    
protected:
    void onLayout() override;
};

} // namespace dodoe

