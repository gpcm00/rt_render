#pragma once
#include <functional>
#include <renderer/window/window.hpp>

class WindowSystem {
  public:
    virtual WindowResult<WindowHandle> create_window(int width, int height) = 0;
    virtual void destroy_window(WindowHandle) = 0;
    virtual void set_title(WindowHandle, std::string title) = 0;
    virtual WindowResult<WindowHandle> get_active_window() = 0;
    virtual size_t num_windows() const = 0;
    virtual void register_create_window_callback(
        std::function<void(WindowHandle)> callback) = 0;
};
