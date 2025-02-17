#include <iostream>
#include <renderer/app.hpp>

class Renderer: public App {


public:
    Renderer() {
        std::cout << "Renderer created" << std::endl;
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