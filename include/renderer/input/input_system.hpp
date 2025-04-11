#include <iostream>
#include <memory>
#include <renderer/input/input.hpp>
#include <renderer/input/keyboard.hpp>
#include <renderer/input/mouse.hpp>
#include <string>
#include <unordered_map>


class WindowSystem;

class InputSystem {
  private:
    WindowSystem *window_system;
    HandleManager<input::ActionHandle> action_handles;
    std::unique_ptr<KeyboardInterface> keyboard_interface;
    std::unique_ptr<MouseInterface> mouse_interface;
    std::unordered_map<input::Key, std::vector<input::KeyBinding>> key_bindings;
    std::unordered_map<input::MouseButton,
                       std::vector<input::MouseButtonBinding>>
        mouse_button_bindings;
    std::unordered_map<std::string, input::ActionHandle> action_names;
    std::unordered_map<input::ActionHandle, input::Action,
                       HandleHasher<input::ActionHandle>>
        actions;

    // private methods
    void reset_actions();
    void evaluate_key_events();
    void evaluate_mouse_events();
    /*
            Checks if action associated with action_name already exists and
       returns the handle. If a handle does not exist, it creates the action.
    */
    input::ActionHandle get_action(std::string action_name);

  public:
    InputSystem(WindowSystem *window_system,
                KeyboardInterface *keyboard_interface,
                MouseInterface *mouse_interface);
    ~InputSystem();
    void reload_controls();
    bool update();
    void create_key_action_binding(std::string action_name, input::Key key,
                                   bool invert = false);
    void create_mouse_button_action_binding(std::string action_name,
                                            input::MouseButton mouse_button,
                                            bool invert = false);
    input::ButtonState get_button_state(std::string action_name);
    input::MousePosition get_mouse_position();
    double get_axis(std::string action_name);
};
