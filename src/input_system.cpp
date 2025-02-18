#include <renderer/input/input_system.hpp>

void InputSystem::reset_actions()
{
	for (auto it = actions.begin(); it != actions.end(); it++) {
		it->second.axis = 0.0;
		it->second.button_state = input::ButtonState::Released;
	}
}

double InputSystem::get_axis(std::string action_name) 
{
	auto it = action_names.find(action_name);
	if (it != action_names.end()) 
	{
		return actions[it->second].axis;
	}
	return 0.0;
}


void InputSystem::evaluate_key_events()
{
	// poll keyboard
	auto key_events = keyboard_interface->get_key_events();
	// convert key event to action using key bindings
	for (auto& key_event : key_events) {

		for (auto& binding : key_bindings[key_event.key]) {

			float axis = 0.0f;
			if ((key_event.button_state == input::ButtonState::Pressed || key_event.button_state == input::ButtonState::Held)) {
				if (binding.inverted) {
					axis = -1.0f;
				}
				else {
					axis = 1.0f;
				}
			}

			actions[binding.action] = { key_event.button_state, axis };
			break;
		}
	}
}

void InputSystem::evaluate_mouse_events()
{
	// poll keyboard
	auto mouse_events = mouse_interface->get_mouse_events();
	// convert key event to action using key bindings
	for (auto& mouse_event : mouse_events) {

		for (auto& binding : mouse_button_bindings[mouse_event.button]) {

			float axis = 0.0f;
			if (mouse_event.button_state == input::ButtonState::Pressed || mouse_event.button_state == input::ButtonState::Held) {
				if (binding.inverted) {
					axis = -1.0f;
				}
				else {
					axis = 1.0f;
				}
			}
			actions[binding.action] = { mouse_event.button_state, axis };
			break;
		}
	}
}

input::ActionHandle InputSystem::get_action(std::string action_name)
{
	auto it = action_names.find(action_name);
	if (it != action_names.end()) {
		return it->second;
	}

	auto handle = action_handles.get();
	action_names[action_name] = handle;
	return handle;

}

InputSystem::InputSystem(WindowSystem* window_system, KeyboardInterface* keyboard_interface, MouseInterface* mouse_interface) 
: window_system(window_system), keyboard_interface(keyboard_interface), mouse_interface(mouse_interface)
{
	create_key_action_binding("Exit", input::Key::Escape);
	create_mouse_button_action_binding("Mouse Action", input::MouseButton::RightMouse);
	create_key_action_binding("Forward", input::Key::W);
	create_key_action_binding("Forward", input::Key::S, true);
	create_key_action_binding("Right", input::Key::D);
	create_key_action_binding("Right", input::Key::A, true);
	// ReloadControls();

}

InputSystem::~InputSystem() {
	keyboard_interface.reset();
}

void InputSystem::reload_controls()
{
	actions.clear();
}

input::ButtonState InputSystem::get_button_state(std::string action_name) {
	// temporary code to refactor later
	auto it = action_names.find(action_name);
	if (it != action_names.end()) {
		auto action_it = actions.find(it->second);
		if (action_it != actions.end()) {
			return action_it->second.button_state;
		}
	}
	return input::ButtonState::Released;
}

input::MousePosition InputSystem::get_mouse_position()
{
	return input::MousePosition(mouse_interface->get_x(), mouse_interface->get_y());
}

bool InputSystem::update()
{
	// put all actions to rest
	// resetActions();
	keyboard_interface->pre_update();
	mouse_interface->pre_update();

	// evaluate device events
	evaluate_key_events();
	evaluate_mouse_events();

	mouse_interface->post_update();
	keyboard_interface->post_update();

	if (get_button_state("Exit") == input::ButtonState::Pressed || get_button_state("Exit") == input::ButtonState::Held) {
		return true;
	}
	if (get_button_state("Mouse Action") == input::ButtonState::Held) {
		std::cout << "\nMouse Action fired!";
	}
	return false;
}
void InputSystem::create_key_action_binding(std::string action_name, input::Key key, bool invert)
{
	// check for existing action handle
	auto action = get_action(action_name);
	// bind key to action		
	auto action_binding = input::KeyBinding{ key, action, invert };
	key_bindings[key].push_back(action_binding);
}

void InputSystem::create_mouse_button_action_binding(std::string action_name, input::MouseButton mouse_button, bool invert)
{
	auto action = get_action(action_name);
	mouse_button_bindings[mouse_button].push_back(input::MouseButtonBinding{ mouse_button, action, invert });
}