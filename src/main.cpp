#include <iostream>
#include <renderer/app.hpp>
#include <renderer/window/window_system_glfw.hpp>
#include <renderer/input/input_system.hpp>
#include <renderer/input/keyboard_glfw.hpp>
#include <renderer/input/mouse_glfw.hpp>
class Renderer: public App {
private:
    WindowSystemGLFW window_system;
    InputSystem input_system;

    std::function<void()> exit_function;

public:
    Renderer(): input_system(&window_system, new KeyboardGLFW(&window_system), new MouseGLFW(&window_system)) {
        std::cout << "Renderer created" << std::endl;

        auto window = window_system.create_window(800, 600);
        window_system.set_title(window.value(), "Renderer");
    }

    ~Renderer() {
        std::cout << "Renderer destroyed" << std::endl;
    }

    void set_exit_function(std::function<void()> function) override {
        exit_function = function;
    }

    void fixed_update(const FrameConstants & frame_constants) override {
        input_system.update();

        if (input_system.get_button_state("Exit") == input::ButtonState::Pressed || input_system.get_button_state("Exit") == input::ButtonState::Held) {
            exit_function();
        }
    }

    void render_update(const FrameConstants & frame_constants) override {
    }
};

int main() {
    std::cout << "Hello, World!" << std::endl;

    Renderer().run();

    return 0;
}