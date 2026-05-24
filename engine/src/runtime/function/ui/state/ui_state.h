#pragma once

namespace engine::core {
    class Context;
}

namespace dodoe {
    class UIInteractive;
}

namespace dodoe {

class UIState {
    friend class dodoe::UIInteractive;
protected:
    dodoe::UIInteractive* owner_ = nullptr;

public:
    UIState(dodoe::UIInteractive* owner) : owner_(owner) {}
    virtual ~UIState() = default;

    UIState(const UIState&) = delete;
    UIState& operator=(const UIState&) = delete;
    UIState(UIState&&) = delete;
    UIState& operator=(UIState&&) = delete;

    virtual bool isHovered() const { return false; }
    
    virtual bool isPressed() const { return false; }

protected:
    virtual void enter() = 0;
    virtual void update(float, engine::core::Context&) {}
    
    virtual void onMouseEnter() {}
    virtual void onMouseExit() {}
    virtual void onMousePressed() {}
    virtual void onMouseReleased(bool /*is_inside*/) {}
};

} // namespace dodoe