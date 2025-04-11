#pragma once
#include <renderer/window/window_system.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <unordered_map>

class Renderer;
class InputSystem;
class WindowSystem;
class KeyboardInterface;
class MouseInterface;

class WindowUserData {
  public:
    WindowUserData() {
        renderer = nullptr;
        // input_system = nullptr;
        window_system = nullptr;
        keyboard_interface = nullptr;
        mouse_interface = nullptr;
    }

    WindowHandle handle;
    Renderer *renderer;
    // InputSystem* input_system;
    WindowSystem *window_system;
    KeyboardInterface *keyboard_interface;
    MouseInterface *mouse_interface;
};

class WindowSystemGLFW final : public WindowSystem {
  private:
    HandleManager<WindowHandle> windowHandles;
    std::unordered_map<WindowHandle, GLFWwindow *, HandleHasher<WindowHandle>>
        windows;
    WindowHandle focused_window;
    std::vector<std::function<void(WindowHandle)>> create_window_callbacks;

  public:
    WindowSystemGLFW() {
        if (!glfwVulkanSupported()) {
            std::runtime_error("\nWarning: Vulkan is not available!");
        }
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }

    ~WindowSystemGLFW() {
        for (auto &it : windows) {
            glfwDestroyWindow(it.second);
        }
        windows.clear();
        windowHandles.reset();
        glfwTerminate();
    }

    size_t num_windows() const override { return windows.size(); }

    WindowResult<WindowHandle> create_window(int width, int height) override {
        auto window = glfwCreateWindow(width, height, "MAG", nullptr, nullptr);
        if (window) {
            auto handle = windowHandles.get();
            windows[handle] = window;
            focused_window = handle;
            // set pointer data
            WindowUserData *wud = new WindowUserData();
            wud->window_system = this;
            glfwSetWindowUserPointer(window, wud);
            // set focused callback
            glfwSetWindowFocusCallback(window, window_focus_callback);

            // lock cursor (testing only)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

            for (auto &callback : create_window_callbacks) {
                callback(handle);
            }
            return {handle, WindowError::Success};
        }
        return {WindowError::Failed};
    };

    void destroy_window(WindowHandle window) override {
        // delete user pointer data
        WindowUserData *wud = static_cast<WindowUserData *>(
            glfwGetWindowUserPointer(windows[window]));
        delete wud;
        glfwDestroyWindow(windows[window]);
        windowHandles.RecycleHandle(window);
        windows.erase(window);
    }
    void set_title(WindowHandle window, std::string title) override {
        glfwSetWindowTitle(windows[window], title.c_str());
    }
    WindowResult<WindowHandle> get_active_window() override {
        for (auto &it : windows) {
            if (glfwGetWindowAttrib(it.second, GLFW_FOCUSED)) {
                return {it.first, WindowError::Success};
            }
        }
        return {WindowError::Failed};
    }

    GLFWwindow *get(WindowHandle window) { return windows[window]; }

    static void window_focus_callback(GLFWwindow *window, int focused) {
        auto wud =
            static_cast<WindowUserData *>(glfwGetWindowUserPointer(window));
        WindowSystemGLFW *window_system =
            dynamic_cast<WindowSystemGLFW *>(wud->window_system);

        if (focused == GLFW_TRUE) {
            window_system->focused_window = wud->handle;
        }
    }

    void register_create_window_callback(
        std::function<void(WindowHandle)> callback) override {
        create_window_callbacks.push_back(callback);
    }
};
