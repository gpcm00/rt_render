#pragma once
#include <iostream>
#include <functional>
#include <renderer/utils.hpp>
#include <renderer/frame_constants.hpp>

class App {
private:
public:

	virtual ~App() {}

	virtual void set_exit_function(std::function<void()> exit_function) = 0;
	virtual void fixed_update(const FrameConstants & frame_constants) = 0;
	virtual void render_update(const FrameConstants & frame_constants) = 0;
	

	void run(const unsigned int update_rate = 50)
	{
		bool exit = false;
		auto last_time = utils::get_time();
		double accumulator = 0.0;
		const double fixed_delta_time = 1.0 / static_cast<double>(update_rate);
		constexpr double max_simulation_delta = 1.0;
		FrameConstants frame_constants = { fixed_delta_time, fixed_delta_time, 0.0 };
		std::function<void()> exit_function = [&exit]() {
			exit = true;
		};

		set_exit_function(exit_function);

		while (!exit) {

			// accumulate delta time
			auto new_time = utils::get_time();
			auto frame_time = new_time - last_time;
			last_time = new_time;
			accumulator += frame_time;

			// clamp accumulator
			if (accumulator > max_simulation_delta) {
				accumulator = max_simulation_delta;
			}

			// timestep integration
			while (accumulator > fixed_delta_time) {
				fixed_update(frame_constants);
				accumulator -= fixed_delta_time;
			}

			// 0.0-1.0 blend factor
			frame_constants.alpha = accumulator / fixed_delta_time;

			render_update(frame_constants);
		}
	}
};