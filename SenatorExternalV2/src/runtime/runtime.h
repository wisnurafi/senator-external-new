#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace runtime
{
	inline std::atomic<bool> g_running{ true };
	inline std::vector<std::thread> g_threads;
	inline std::mutex g_threads_mtx;

	inline bool alive() noexcept
	{
		return g_running.load(std::memory_order_relaxed);
	}

	inline void request_stop() noexcept
	{
		g_running.store(false, std::memory_order_relaxed);
	}

	template <typename Fn>
	inline void spawn(Fn&& fn)
	{
		std::lock_guard<std::mutex> lk(g_threads_mtx);
		g_threads.emplace_back(std::forward<Fn>(fn));
	}

	template <typename Fn>
	inline void spawn_detached(Fn&& fn)
	{
		std::thread(std::forward<Fn>(fn)).detach();
	}

	inline void join_all(std::chrono::milliseconds soak = std::chrono::milliseconds(250))
	{
		request_stop();

		// Let feature loops observe the flag and exit their sleeps.
		std::this_thread::sleep_for(soak);

		std::vector<std::thread> threads;
		{
			std::lock_guard<std::mutex> lk(g_threads_mtx);
			threads = std::move(g_threads);
		}

		// Detach rather than join: any feature loop that hasn't migrated to runtime::alive()
		// would deadlock join(). Threads that did observe the flag have already exited.
		// Anything still mid-iteration after soak gets reaped at process exit.
		for (auto& t : threads)
		{
			if (t.joinable())
				t.detach();
		}
	}
}
