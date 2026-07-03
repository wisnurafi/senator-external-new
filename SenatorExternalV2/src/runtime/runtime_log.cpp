#include "runtime_log.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace
{
	std::mutex g_log_mutex;
	std::ofstream g_log_file;
	std::string g_log_path = "logs\\runtime.log";
	bool g_initialized = false;

	const char* level_name(runtime_log::Level level)
	{
		switch (level)
		{
		case runtime_log::Level::Info: return "INFO";
		case runtime_log::Level::Warning: return "WARN";
		case runtime_log::Level::Error: return "ERROR";
		default: return "INFO";
		}
	}

	std::string timestamp()
	{
		const auto now = std::chrono::system_clock::now();
		const std::time_t now_time = std::chrono::system_clock::to_time_t(now);

		std::tm local_time{};
		localtime_s(&local_time, &now_time);

		std::ostringstream ss;
		ss << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
		return ss.str();
	}
}

bool runtime_log::initialize(const std::string& path)
{
	std::lock_guard<std::mutex> lock(g_log_mutex);

	if (g_initialized)
		return true;

	g_log_path = path;

	try
	{
		const std::filesystem::path log_path(g_log_path);
		if (log_path.has_parent_path())
			std::filesystem::create_directories(log_path.parent_path());

		g_log_file.open(g_log_path, std::ios::out | std::ios::app);
		if (!g_log_file.is_open())
			return false;

		g_initialized = true;
		g_log_file << "\n";
		g_log_file << "------------------------------------------------------------\n";
		g_log_file << "[" << timestamp() << "] [INFO] [Runtime] Session started\n";
		g_log_file.flush();
		return true;
	}
	catch (...)
	{
		return false;
	}
}

void runtime_log::shutdown()
{
	std::lock_guard<std::mutex> lock(g_log_mutex);

	if (g_log_file.is_open())
	{
		g_log_file << "[" << timestamp() << "] [INFO] [Runtime] Session ended\n";
		g_log_file.flush();
		g_log_file.close();
	}

	g_initialized = false;
}

void runtime_log::write(Level level, std::string_view category, std::string_view message)
{
	std::lock_guard<std::mutex> lock(g_log_mutex);

	if (!g_initialized || !g_log_file.is_open())
		return;

	g_log_file
		<< "[" << timestamp() << "] "
		<< "[" << level_name(level) << "] "
		<< "[" << category << "] "
		<< message << "\n";
	g_log_file.flush();
}

void runtime_log::info(std::string_view category, std::string_view message)
{
	write(Level::Info, category, message);
}

void runtime_log::warning(std::string_view category, std::string_view message)
{
	write(Level::Warning, category, message);
}

void runtime_log::error(std::string_view category, std::string_view message)
{
	write(Level::Error, category, message);
}

std::string runtime_log::path()
{
	std::lock_guard<std::mutex> lock(g_log_mutex);
	return g_log_path;
}
