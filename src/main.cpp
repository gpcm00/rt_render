#include <iostream>
#include <renderer/app.hpp>
#include <renderer/window/window_system_glfw.hpp>
class Renderer: public App {
private:
    WindowSystemGLFW window_system;
    std::function<void()> exit_function;

public:
    Renderer() {
        std::cout << "Renderer created" << std::endl;

        auto window = window_system.create_window(800, 600);
        window_system.set_title(window.value(), "Renderer");
    }

    ~Renderer() {
        std::cout << "Renderer destroyed" << std::endl;
    }

    void set_exit_function(std::function<void()> exit_function) override {
        std::cout << "Exit function set" << std::endl;
    }

    void fixed_update(const FrameConstants & frame_constants) override {
    }

    void render_update(const FrameConstants & frame_constants) override {
    }
};

int main() {
    std::cout << "Hello, World!" << std::endl;

    Renderer().run();

    return 0;
}