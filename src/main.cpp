#include <iostream>
#include <memory>
#include <renderer/app.hpp>
#include <renderer/input/input_system.hpp>
#include <renderer/input/keyboard_glfw.hpp>
#include <renderer/input/mouse_glfw.hpp>
#include <renderer/window/window_system_glfw.hpp>

#define VMA_IMPLEMENTATION
#include <renderer/renderer.hpp>

#include <glm/gtc/matrix_transform.hpp>

class PathTracer : public App {
  private:
    WindowSystemGLFW window_system;
    InputSystem input_system;
    std::unique_ptr<Renderer> renderer;

    std::function<void()> exit_function;

    glm::vec3 camera_position;
    glm::vec3 camera_direction;

    input::MousePosition last_mouse_position;
    float pitch;
    float yaw;

  public:
    PathTracer(const std::filesystem::path scene_path)
        : input_system(&window_system, new KeyboardGLFW(&window_system),
                       new MouseGLFW(&window_system)),
          last_mouse_position(0, 0) {
        std::cout << "PathTracer created" << std::endl;

        // Hard code the dimensions for now
        const auto [width, height] = renderer->get_dimensions();
        auto window = window_system.create_window(width, height);
        window_system.set_title(window.value(), "Vulkan Path Tracer");

        renderer = std::make_unique<Renderer>(window.value(), &window_system, scene_path);

        camera_position = {5.0f, 5.0f, 5.0f};
        glm::vec3 target = {0.0f, 0.0f, 0.0f};
        glm::vec3 up = {0.0f, 1.0f, 0.0f};
        camera_direction = glm::normalize(target - camera_position);

        // Set up camera controls
        input_system.create_key_action_binding("Forward", input::Key::W, false);
        input_system.create_key_action_binding("Forward", input::Key::S, true);
        input_system.create_key_action_binding("Right", input::Key::S, false);
        input_system.create_key_action_binding("Right", input::Key::A, true);
        input_system.create_key_action_binding("ToggleAveraging", input::Key::F,
                                               false);

        last_mouse_position = input_system.get_mouse_position();

        // Compute initial pitch and yaw from camera direction
        pitch = glm::degrees(asin(camera_direction.z));
        yaw = glm::degrees(atan2(camera_direction.y, camera_direction.x));
        pitch = glm::clamp(pitch, -89.9f, 89.9f);
        yaw = glm::mod(yaw, 360.0f);

        // Initialize camera
        auto &camera = renderer->get_camera();
        camera.set_position(camera_position);
        camera.set_direction(camera_direction);
        camera.set_up(up);
        camera.set_right(glm::normalize(glm::cross(camera_direction, up)));
    }

    ~PathTracer() { std::cout << "PathTracer destroyed" << std::endl; }

    void set_exit_function(std::function<void()> function) override {
        exit_function = function;
    }

    void fixed_update(const FrameConstants &frame_constants) override {}

    void render_update(const FrameConstants &frame_constants) override {

        input_system.update();
        // Update camera
        auto &camera = renderer->get_camera();
        auto up = glm::vec3(0.0f, 1.0f, 0.0f);

        // Update camera direction based on mouse movement
        auto mouse_position = input_system.get_mouse_position();
        float delta_x = mouse_position.x - last_mouse_position.x;
        float delta_y = mouse_position.y - last_mouse_position.y;
        last_mouse_position = mouse_position;

        // Sometimes jumps after first few frames
        if (abs(delta_x) > 100.0f || abs(delta_y) > 100.0f) {
            delta_x = 0.0f;
            delta_y = 0.0f;
        }

        yaw += delta_x * 0.1f;
        pitch -= delta_y * 0.1f;
        pitch = glm::clamp(pitch, -89.9f, 89.9f);
        yaw = glm::mod(yaw, 360.0f);

        // Local variables will hold the new values
        camera_direction.x = cos(glm::radians(pitch)) * cos(glm::radians(yaw));
        camera_direction.y = sin(glm::radians(pitch));
        camera_direction.z = cos(glm::radians(pitch)) * sin(glm::radians(yaw));
        camera_direction = glm::normalize(camera_direction);
        auto new_right =
            glm::vec4(glm::normalize(glm::cross(glm::vec3(camera_direction),
                                                glm::vec3(up))),
                      0.0f);

        // Update camera position using old variables
        camera_position +=
            static_cast<float>(input_system.get_axis("Forward")) *
            glm::vec3(camera.direction) * 0.1f;
        camera_position += static_cast<float>(input_system.get_axis("Right")) *
                           glm::vec3(camera.right) * 0.1f;

        if (delta_x != 0.0f || delta_y != 0.0f ||
            input_system.get_axis("Forward") != 0.0f ||
            input_system.get_axis("Right") != 0.0f) {
            renderer->set_camera_changed(true);
        }

        if (input_system.get_button_state("Exit") ==
                input::ButtonState::Pressed ||
            input_system.get_button_state("Exit") == input::ButtonState::Held) {
            exit_function();
        }

        // Update old parameters with local ones
        camera.set_position(camera_position);
        camera.set_direction(camera_direction);
        camera.set_up(up);
        camera.set_right(new_right);

        camera.set_fov(110.0f);
        camera.set_range(0.001f, 10000.0f);

        auto [r_width, r_height] = renderer->get_dimensions();
        camera.aspect_ratio =
            static_cast<float>(r_width) / static_cast<float>(r_height);
        // Render
        renderer->render(frame_constants);
    }
};

int main(int argc, char *argv[]) {

    std::filesystem::path scene_path;
    if (argc < 2) {
        std::cout << "No scene path provided. Using default scene." << std::endl;
        scene_path = "glTF-Sample-Assets/Models/ABeautifulGame/glTF/ABeautifulGame.gltf";
    }
    else {
        std::cout << "Using scene path: " << argv[1] << std::endl;
        scene_path = argv[1];
    }

    PathTracer(scene_path).run();

    return 0;
}
