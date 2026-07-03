#include "rapidfire.h"

#include <game/game.h>
#include <cache/cache.h>
#include <sdk/sdk.h>
#include <memory/memory.h>
#include <Offsets/Offsets.hpp>
#include <settings.h>

#include <windows.h>
#include <mmsystem.h>
#include <thread>
#include <chrono>
#include <mutex>

void rage::rapidfire::run()
{
	bool timer_resolution_set = false;
	auto release_timer_resolution = [&]() {
		if (timer_resolution_set)
		{
			timeEndPeriod(1);
			timer_resolution_set = false;
		}
	};

	while (runtime::alive()) {
		if (settings::rage::rapidfire == false) {
			release_timer_resolution();
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			continue;
		}

		if (!game::local_player.address) {
			release_timer_resolution();
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			continue;
		}

		rbx::instance_t backpack = game::local_player.find_first_child("Backpack");
		if (!backpack.address) {
			release_timer_resolution();
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			continue;
		}

		std::vector<rbx::instance_t> tools = backpack.get_children();
		for (rbx::instance_t tool : tools) {
			if (tool.get_class_name() != "Tool" && tool.get_name() != "Combat") continue;

			rbx::instance_t shooting = tool.find_first_child("ShootingCooldown");
			rbx::instance_t tolerance = tool.find_first_child("ToleranceCooldown");

			if (shooting.address) {
				shooting.set_instance_double(0.0001);
			}
			if (tolerance.address) {
				tolerance.set_instance_double(0.0001);
			}
		}

		static LARGE_INTEGER frequency;
		static LARGE_INTEGER lastTime;
		static bool timeInitialized = false;

		if (!timeInitialized) {
			QueryPerformanceFrequency(&frequency);
			QueryPerformanceCounter(&lastTime);
			timeInitialized = true;
		}

		if (!timer_resolution_set)
		{
			timeBeginPeriod(1);
			timer_resolution_set = true;
		}

		const double targetFrameTime = 1.0 / 800;

		LARGE_INTEGER currentTime;
		QueryPerformanceCounter(&currentTime);
		double elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;

		if (elapsedSeconds < targetFrameTime) {
			DWORD sleepMilliseconds = static_cast<DWORD>((targetFrameTime - elapsedSeconds) * 1000.0);
			if (sleepMilliseconds > 0) {
				Sleep(sleepMilliseconds);
			}
		}

		do {
			QueryPerformanceCounter(&currentTime);
			elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
		} while (elapsedSeconds < targetFrameTime);

		lastTime = currentTime;

		SwitchToThread();
	}

	release_timer_resolution();
}
