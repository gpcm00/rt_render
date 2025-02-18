#pragma once
#include <vector>
template<class T>
class HandleManager {
private:
	std::vector<T> reusableHandles;
	T nextHandle;
public:

	HandleManager() {
		nextHandle.value = 0;
	}

	void RecycleHandle(T handle) {
		reusableHandles.push_back(handle);
	}

	T get() {

		if (reusableHandles.size() > 0) {
			nextHandle = reusableHandles.back();
			reusableHandles.pop_back();
		}

		auto newHandle = nextHandle;
		nextHandle.value++;
		return newHandle;
	}

	void reset() {
		nextHandle.value = 0;
		reusableHandles.clear();
	}

};
