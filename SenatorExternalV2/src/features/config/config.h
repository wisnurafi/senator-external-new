#pragma once
#include <string>
#include <vector>

namespace config
{
	struct config_info_t
	{
		std::string name;
		std::string path;
	};

	std::string get_config_directory();
	std::string get_config_path(const std::string& name);
	bool ensure_config_directory();

	std::vector<config_info_t> get_config_list();
	bool save_config(const std::string& name);
	bool load_config(const std::string& name);
	bool delete_config(const std::string& name);
	bool config_exists(const std::string& name);
	void open_file_location();
	void reset_to_defaults();
	int current_config_version();
}

namespace external_config
{
	inline std::string cheat_name = "Senator External";
	inline std::string autoload_config = "";
#ifdef _DEBUG
	inline bool hide_console = false;
#else
	inline bool hide_console = true;
#endif
	inline std::string offsets_url = "";        // legacy: full URL to offsets.json
	inline std::string offsets_base_url = "https://offsets.imtheo.lol"; // service base: probes /roblox/version, fetches /offsets.json

	void load();
	void save();
	void ensure();
}
