#pragma once

#include "dopch.h"
#include "interaction_behavior.h"

namespace dodoe {

class DragBehavior final : public InteractionBehavior {
public:
    using DragBeginCallback = std::function<void(UIInteractive&, const Vector2f&)>;
    using DragUpdateCallback = std::function<void(UIInteractive&, const Vector2f&, const Vector2f&)>;
    using DragEndCallback = std::function<void(UIInteractive&, const Vector2f&, bool)>;

private:
    DragBeginCallback on_begin_{};
    DragUpdateCallback on_update_{};
    DragEndCallback on_end_{};

public:
    DragBehavior() = default;
    ~DragBehavior() override = default;

    void setOnBegin(DragBeginCallback cb) { on_begin_ = std::move(cb); }
    void setOnUpdate(DragUpdateCallback cb) { on_update_ = std::move(cb); }
    void setOnEnd(DragEndCallback cb) { on_end_ = std::move(cb); }

    void onDragBegin(UIInteractive& owner, const Vector2f& pos) override {
        if (on_begin_) on_begin_(owner, pos);
    }

    void onDragUpdate(UIInteractive& owner, const Vector2f& pos, const Vector2f& delta) override {
        if (on_update_) on_update_(owner, pos, delta);
    }

    void onDragEnd(UIInteractive& owner, const Vector2f& pos, bool accepted) override {
        if (on_end_) on_end_(owner, pos, accepted);
    }
};

} // namespace dodoe
