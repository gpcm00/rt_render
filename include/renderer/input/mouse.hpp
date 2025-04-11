#pragma once
#include <renderer/input/input.hpp>
#include <vector>


namespace input {

enum class MouseButton { LeftMouse, RightMouse, MiddleMouse, Unknown };

struct MouseEvent {
    MouseButton button;
    ButtonState button_state;
};

class MousePosition {
  public:
    MousePosition(double x, double y) : x(x), y(y) {}

    double x;
    double y;
};

struct MouseButtonBinding {
    // The mouse button used for this binding.
    MouseButton button;
    // The name of the action.
    ActionHandle action;
    // Specifies if binding has a positive or negative influence on the axis.
    bool inverted;
};

} // namespace input

class MouseInterface {
  public:
    virtual ~MouseInterface() {};
    virtual const std::vector<input::MouseEvent> &get_mouse_events() = 0;
    virtual void post_update() = 0;
    virtual void pre_update() = 0;
    virtual double get_x() = 0;
    virtual double get_y() = 0;
};
