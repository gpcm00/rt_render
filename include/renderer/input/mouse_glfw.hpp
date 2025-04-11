#pragma once
#include <renderer/input/mouse.hpp>
#include <renderer/window/window_system_glfw.hpp>
#include <unordered_map>

class MouseGLFW : public MouseInterface {
  private:
    WindowSystemGLFW *window_system;
    std::unordered_map<int, input::MouseButton> mouse_button_map;
    std::vector<input::MouseEvent> mouse_events;
    std::vector<input::MouseEvent> previous_mouse_events;
    double position_x;
    double position_y;

  public:
    MouseGLFW(WindowSystem *window_system) : position_x(0), position_y(0) {
        this->window_system = dynamic_cast<WindowSystemGLFW *>(window_system);
        window_system->register_create_window_callback(
            [&](WindowHandle handle) { onCreateWindow(handle); });
        using namespace input;
        mouse_button_map = {
            {GLFW_MOUSE_BUTTON_1, MouseButton::LeftMouse},
            {GLFW_MOUSE_BUTTON_2, MouseButton::RightMouse},
            {GLFW_MOUSE_BUTTON_3, MouseButton::MiddleMouse},
            // support more later
        };
    }

    virtual void pre_update() override {
        auto window_handle = window_system->get_active_window();
        if (window_handle) {
            auto window = window_system->get(window_handle.value());
            glfwGetCursorPos(window, &position_x, &position_y);
        }
    }

    virtual const std::vector<input::MouseEvent> &get_mouse_events() override {
        // check if we need to convert pressed event to held event
        for (const auto &last_event : previous_mouse_events) {
            if (last_event.button_state == input::ButtonState::Pressed ||
                last_event.button_state == input::ButtonState::Held) {
                // find
                auto it =
                    std::find_if(mouse_events.cbegin(), mouse_events.cend(),
                                 [&](const input::MouseEvent &event) {
                                     return event.button == last_event.button;
                                 });

                if (it == mouse_events.end()) {
                    mouse_events.push_back(
                        {last_event.button, input::ButtonState::Held});
                }
            }
        }
        return mouse_events;
    }

    virtual void post_update() override {
        previous_mouse_events = mouse_events;
        mouse_events.clear();
    }

    void onCreateWindow(WindowHandle handle) {
        auto window = window_system->get(handle);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        auto window_user_data =
            static_cast<WindowUserData *>(glfwGetWindowUserPointer(window));
        window_user_data->mouse_interface = this;
    }

    static void mouseButtonCallback(GLFWwindow *window, int button, int action,
                                    int mods) {
        auto window_user_data =
            static_cast<WindowUserData *>(glfwGetWindowUserPointer(window));
        auto mouse_interface =
            dynamic_cast<MouseGLFW *>(window_user_data->mouse_interface);

        input::MouseEvent event;

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

        // look up button

        auto it = mouse_interface->mouse_button_map.find(button);

        if (it != mouse_interface->mouse_button_map.end()) {
            event.button = it->second;
        } else {
            event.button = input::MouseButton::Unknown;
        }

        mouse_interface->mouse_events.push_back(event);
    }

    double get_x() override { return position_x; }

    double get_y() override { return position_y; }
};
