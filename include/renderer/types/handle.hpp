#pragma once
#include <renderer/types/handle_manager.hpp>

template <typename T = uint64_t> class Handle {
  public:
    T value;

    bool operator==(const Handle &b) const { return value == b.value; }

    size_t hash() const { return std::hash<T>()(value); }
};

template <typename T> struct HandleHasher {
    size_t operator()(const T &handle) const { return handle.hash(); }
};
