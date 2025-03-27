#include <iostream>
#include <renderer/app.hpp>
#include <renderer/window/window_system_glfw.hpp>
#include <renderer/input/input_system.hpp>
#include <renderer/input/keyboard_glfw.hpp>
#include <renderer/input/mouse_glfw.hpp>
#include <memory>

#include <renderer/renderer.hpp>

// #define VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

class PathTracer: public App {
private:
    WindowSystemGLFW window_system;
    InputSystem input_system;
    std::unique_ptr<Renderer> renderer;

    std::function<void()> exit_function;

public:
    PathTracer(): input_system(&window_system, new KeyboardGLFW(&window_system), new MouseGLFW(&window_system)) {
        std::cout << "PathTracer created" << std::endl;

        auto window = window_system.create_window(800, 600);
        window_system.set_title(window.value(), "Vulkan Path Tracer");

        renderer = std::make_unique<Renderer>(window.value(), &window_system);
    }

    ~PathTracer() {
        std::cout << "PathTracer destroyed" << std::endl;
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

        renderer->render(frame_constants);
    }
};

int main() {

    PathTracer().run();

    return 0;
}