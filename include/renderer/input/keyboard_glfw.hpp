#pragma once
#include <renderer/input/keyboard.hpp>
#include <renderer/window/window_system_glfw.hpp>
#include <iostream>
#include <unordered_map>

class WindowSystem;

class KeyboardGLFW : public KeyboardInterface {
private:
	WindowSystemGLFW* window_system;
	std::vector<input::KeyEvent> key_events;
	std::unordered_map<decltype(GLFW_KEY_A), input::Key> keymap;
public:

	KeyboardGLFW(WindowSystem* window_system) {
		// get window system
		this->window_system = dynamic_cast<WindowSystemGLFW*>(window_system);
		this->window_system->register_create_window_callback([&](WindowHandle handle) {onCreateWindow(handle); });
		keymap_init();
		std::cout << "Initialized GLFW Keyboard Interface!" << std::endl;

	}

	virtual ~KeyboardGLFW() {
		std::cout << "Deinitializing GLFW Keyboard Interface!" << std::endl;
	};

	virtual const std::vector<input::KeyEvent>& get_key_events() override {
		return key_events;
	}

	virtual void pre_update() override {
		glfwPollEvents();
	}

	virtual void post_update() override {
		key_events.clear();
	}

	input::Key lookup_key(int glfw_key) {
		auto it = keymap.find(glfw_key);
		if (it == keymap.end()) {
			return input::Key::Unknown;
		}
		return it->second;
	}

	void onCreateWindow(WindowHandle handle) {

		auto window = window_system->get(handle);
		glfwSetKeyCallback(window, key_callback);

		// set keyboard interface in window user pointer
		auto window_user_data = static_cast<WindowUserData*>(glfwGetWindowUserPointer(window));
		window_user_data->keyboard_interface = this;
	}

	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
		auto window_user_data = static_cast<WindowUserData*>(glfwGetWindowUserPointer(window));
		auto keyboard_interface = static_cast<KeyboardGLFW*>(window_user_data->keyboard_interface);
		// generate key event
		input::KeyEvent event;
		// get event type
		switch (action) {
		case GLFW_PRESS:
			event.button_state = input::ButtonState::Pressed;
			break;
		case GLFW_RELEASE:
			event.button_state = input::ButtonState::Released;
			break;
		case GLFW_REPEAT:
			event.button_state = input::ButtonState::Held;
			break;
		default:
			event.button_state = input::ButtonState::Released;
			break;
		}

		// get key
		event.key = keyboard_interface->lookup_key(key);

		// store event
		keyboard_interface->key_events.push_back(event);
	}

	void keymap_init() {
		using namespace input;
		keymap = {
		{GLFW_KEY_A, Key::A},
		{GLFW_KEY_B, Key::B},
		{GLFW_KEY_C, Key::C},
		{GLFW_KEY_D, Key::D},
		{GLFW_KEY_E, Key::E},
		{GLFW_KEY_F, Key::F},
		{GLFW_KEY_G, Key::G},
		{GLFW_KEY_H, Key::H},
		{GLFW_KEY_I, Key::I},
		{GLFW_KEY_J, Key::J},
		{GLFW_KEY_K, Key::K},
		{GLFW_KEY_L, Key::L},
		{GLFW_KEY_M, Key::M},
		{GLFW_KEY_N, Key::N},
		{GLFW_KEY_O, Key::O},
		{GLFW_KEY_P, Key::P},
		{GLFW_KEY_Q, Key::Q},
		{GLFW_KEY_R, Key::R},
		{GLFW_KEY_S, Key::S},
		{GLFW_KEY_T, Key::T},
		{GLFW_KEY_U, Key::U},
		{GLFW_KEY_V, Key::V},
		{GLFW_KEY_W, Key::W},
		{GLFW_KEY_X, Key::X},
		{GLFW_KEY_Y, Key::Y},
		{GLFW_KEY_Z, Key::Z},
		{GLFW_KEY_ENTER, Key::Enter},
		{GLFW_KEY_SPACE, Key::Space},
		{GLFW_KEY_ESCAPE, Key::Escape},
		{GLFW_KEY_LEFT_SHIFT, Key::LeftShift},
		{GLFW_KEY_LEFT_CONTROL, Key::LeftControl},
		{GLFW_KEY_LEFT_ALT, Key::LeftAlt}
		};
	}
};