#pragma once
#include <renderer/types/handle.hpp>
#include <renderer/types/result.hpp>
#include <renderer/types/error.hpp>

class WindowHandle: public Handle<> {};

enum class WindowError { Success, NoWindowsExist, Failed};

template <typename T>
using WindowResult = result::Result<T, error::Status<WindowError>>;