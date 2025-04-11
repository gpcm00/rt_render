#pragma once
#include <renderer/types/handle.hpp>
#include <string>

namespace input {

/*
        Unique identifier for action. Needs to be synchronized / mapped to
   server's action identifiers.
*/
class ActionHandle : public Handle<uint16_t> {};

/*
        Represents button state.
*/
enum class ButtonState : uint16_t {
    Pressed,
    Released,
    Held,
};

/*
        Represents an input action.
*/
struct Action {

    // An opaque handle determined at runtime. Should not be managed directly.
    // ActionHandle handle;

    // The button state of this action.
    ButtonState button_state;

    // May represent normalized axis strength or mouse position or delta on one
    // axis.
    float axis;
};

} // namespace input
