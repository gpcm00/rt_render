#pragma once

namespace result {
template <typename ValueType, typename ErrorType> class Result {
  protected:
    ValueType m_value;
    ErrorType m_error;

  public:
    constexpr bool ok() const { return m_error.ok(); };
    constexpr operator bool() const { return ok(); };
    const constexpr ValueType &value() const {
        if (!m_error.ok()) {
            throw std::runtime_error("Error: Invalid result was accessed!");
        }
        return m_value;
    };
    constexpr Result(ValueType value, ErrorType error)
        : m_value(value), m_error(error) {};
    constexpr Result(ErrorType error) : m_error(error) {};

    template <typename Function>
    constexpr Result then(const Function function) {
        if (ok()) {
            return function(*this);
        }
        return *this;
    };
};
} // namespace result
