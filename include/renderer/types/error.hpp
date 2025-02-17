#pragma once

namespace error {
	template<typename T>
	class Status {
	public:
		T error;
		Status(T error) : error(error) {};
		constexpr bool ok() const { return error == T::Success; };
	};
}