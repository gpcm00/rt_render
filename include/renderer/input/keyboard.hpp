#pragma once
#include <memory>
#include <vector>
#include <renderer/input/input.hpp>
#include <renderer/input/keys.hpp>

namespace input {


	struct KeyEvent {
		Key key;
		ButtonState button_state;
	};


	struct KeyBinding {
		// The keyboard key.
		Key key;
		// The name of the action.
		ActionHandle action;
		// Specifies if binding has a positive or negative influence on the axis.
		bool inverted;
	};
}

class KeyboardInterface {
	public:

		virtual ~KeyboardInterface() {};
		virtual const std::vector<input::KeyEvent>& get_key_events() = 0;
		virtual void pre_update() = 0;
		virtual void post_update() = 0;
};