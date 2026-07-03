#pragma once

#include <string>
#include <string_view>

namespace runtime_log
{
	enum class Level
	{
		Info,
		Warning,
		Error,
	};

	bool initialize(const std::string& path = "logs\\runtime.log");
	void shutdown();
	void write(Level level, std::string_view category, std::string_view message);
	void info(std::string_view category, std::string_view message);
	void warning(std::string_view category, std::string_view message);
	void error(std::string_view category, std::string_view message);
	std::string path();
}
