#include "include.hxx"
#include <Offsets/OffsetLoader.hpp>
#include <branding/branding.h>
#include <cache/game_detection.h>
#include <features/config/config.h>
#include <loader/loader.h>
#include <runtime/runtime.h>
#include <runtime/runtime_log.h>
#include <utils/net/https_get.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace console_status
{
    inline std::string current_section = "Runtime";

    inline void print_line()
    {
        printf("------------------------------------------------------------\n");
    }

    inline void banner(const std::string& title)
    {
        printf("\n");
        print_line();
        printf("  %s\n", title.c_str());
        print_line();
        printf("\n");
        runtime_log::info("Runtime", title + " started");
    }

    inline void section(const char* title)
    {
        current_section = title != nullptr ? title : "Runtime";
        printf("\n[%s]\n", title);
        runtime_log::info("Section", current_section);
    }

    inline void statusf(const char* label, const char* fmt, ...)
    {
        char message[1024]{};

        va_list args;
        va_start(args, fmt);
        vsnprintf(message, sizeof(message), fmt, args);
        va_end(args);

        printf("  [%-5s] ", label);
        printf("%s", message);
        printf("\n");

        runtime_log::Level level = runtime_log::Level::Info;
        if (label != nullptr && std::strcmp(label, "WARN") == 0)
            level = runtime_log::Level::Warning;
        else if (label != nullptr && std::strcmp(label, "ERROR") == 0)
            level = runtime_log::Level::Error;

        runtime_log::write(level, current_section, message);
    }

    inline void action(const char* message)
    {
        printf("\n  ACTION: %s\n", message);
        runtime_log::write(runtime_log::Level::Warning, current_section, std::string("ACTION: ") + (message != nullptr ? message : ""));
    }

    inline bool has_console()
    {
        return GetConsoleWindow() != nullptr;
    }

    inline void wait_for_exit_ack()
    {
        if (has_console())
        {
            printf("\nPress ENTER to exit...\n");
            std::cin.get();
            return;
        }

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

namespace
{
    struct startup_game_state_t
    {
        std::uint64_t fake_datamodel{};
        std::uint64_t game_id{};
        std::uint64_t place_id{};
        std::uint64_t local_player{};
        std::uint64_t local_character{};
        gamesupport::Detection detection{};
    };

    bool capture_startup_game_state(startup_game_state_t& state)
    {
        state = {};

        try
        {
            state.fake_datamodel = memory->read<std::uint64_t>(memory->get_module_address() + Offsets::FakeDataModel::Pointer);
            if (state.fake_datamodel == 0)
                return false;

            game::datamodel = rbx::instance_t(memory->read<std::uint64_t>(state.fake_datamodel + Offsets::FakeDataModel::RealDataModel));
            if (game::datamodel.address == 0)
                return false;

            state.game_id = memory->read<std::uint64_t>(game::datamodel.address + Offsets::DataModel::GameId);
            state.place_id = memory->read<std::uint64_t>(game::datamodel.address + Offsets::DataModel::PlaceId);
            state.detection = gamesupport::detect(state.game_id, state.place_id);
            game::set_active_detection(state.detection);

            if (state.game_id == 0 && state.place_id == 0)
                return false;

            game::players = game::datamodel.find_first_child_by_class("Players");
            game::workspace = game::datamodel.find_first_child_by_class("Workspace");
            if (game::players.address == 0 || game::workspace.address == 0)
                return false;

            game::visengine = { memory->read<std::uint64_t>(memory->get_module_address() + Offsets::VisualEngine::Pointer) };
            if (game::visengine.address == 0)
                return false;

            game::camera = memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera);
            if (game::camera == 0)
                return false;

            state.local_player = memory->read<std::uint64_t>(game::players.address + Offsets::Player::LocalPlayer);
            if (state.local_player == 0)
                return false;

            state.local_character = memory->read<std::uint64_t>(state.local_player + Offsets::Player::ModelInstance);
            if (state.local_character == 0 && state.detection.key != gamesupport::GameKey::Overkill)
                return false;
        }
        catch (...)
        {
            return false;
        }

        return true;
    }
}

int main()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (!loader::run())
        return 0;

    external_config::load();
    runtime_log::initialize();

    console_status::banner(branding::product_full_name);

    static const char* BINARY_CANDIDATES[] = {
        "RobloxPlayerBeta.exe",      // Win32 / Bloxstrap
        "Windows10Universal.exe",    // Microsoft Store (UWP) build
    };
    constexpr size_t BINARY_CANDIDATE_COUNT = sizeof(BINARY_CANDIDATES) / sizeof(BINARY_CANDIDATES[0]);
    const char* BINARY_NAME = nullptr;

    console_status::section("Startup");
    console_status::statusf("WAIT", "Waiting for Roblox to open...");
    while (!BINARY_NAME)
    {
        for (size_t i = 0; i < BINARY_CANDIDATE_COUNT; ++i)
        {
            if (memory->find_process_id(BINARY_CANDIDATES[i]))
            {
                BINARY_NAME = BINARY_CANDIDATES[i];
                break;
            }
        }
        if (!BINARY_NAME)
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    while (!memory->attach_to_process(BINARY_NAME))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    while (!memory->find_module_address(BINARY_NAME))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    console_status::statusf("OK", "Connected to Roblox (%s)", BINARY_NAME);

    game::binary_name = BINARY_NAME;

    // Load offsets from JSON (creates offsets.json if missing)
    int offsetResult = OffsetLoader::LoadOffsets();
    if (offsetResult == -1) {
        console_status::statusf("WARN", "Local offsets.json is invalid. Using built-in offsets for now.");
    }

    // Check Roblox version
    std::string robloxVersion = OffsetLoader::GetRobloxVersionFromProcess(memory->get_process_handle(), memory->get_process_id());

    bool version_mismatch = false;

    // Auto-update: GET {base}/roblox/version, compare to cached. If they differ
    // (or no cache), GET {base}/offsets.json and reload. One tiny request on the
    // happy path; full download only on Roblox update.
    auto probe_and_sync_offsets = [&]() -> bool {
        const std::string& base = external_config::offsets_base_url;
        if (base.empty()) return false;

        std::string base_trimmed = base;
        while (!base_trimmed.empty() && base_trimmed.back() == '/')
            base_trimmed.pop_back();

        const std::string version_url = base_trimmed + "/roblox/version";
        const std::string offsets_url = base_trimmed + "/offsets.json";

        console_status::section("Offsets");
        console_status::statusf("INFO", "Checking for offset updates...");

        std::string remote_version_raw;
        if (!netutil::https_get(version_url, remote_version_raw)) {
            console_status::statusf("WARN", "Offset update server is unreachable. Continuing with local offsets.");
            return false;
        }

        std::string remote_version;
        try {
            auto j = nlohmann::json::parse(remote_version_raw);
            if (j.is_string())            remote_version = j.get<std::string>();
            else if (j.contains("version") && j["version"].is_string())
                remote_version = j["version"].get<std::string>();
            else if (j.contains("ClientVersion") && j["ClientVersion"].is_string())
                remote_version = j["ClientVersion"].get<std::string>();
        }
        catch (...) {
            remote_version = remote_version_raw;
        }
        while (!remote_version.empty() && (remote_version.back() == '\n' || remote_version.back() == '\r' || remote_version.back() == ' ' || remote_version.back() == '"'))
            remote_version.pop_back();
        while (!remote_version.empty() && (remote_version.front() == ' ' || remote_version.front() == '"'))
            remote_version.erase(remote_version.begin());

        if (remote_version.empty()) {
            console_status::statusf("WARN", "Offset update server returned an unreadable version. Continuing with local offsets.");
            return false;
        }

        const std::string cached_version = OffsetLoader::WasLoadedFromJson()
            ? OffsetLoader::GetExpectedVersion()
            : std::string{};

        if (!cached_version.empty() && cached_version == remote_version) {
            console_status::statusf("OK", "Offsets are up to date (%s).", cached_version.c_str());
            return false;
        }

        console_status::statusf("INFO", "New offsets available. Downloading latest offsets...");

        std::string body;
        if (!netutil::https_get(offsets_url, body)) {
            console_status::statusf("WARN", "Offset download failed. Continuing with local offsets.");
            return false;
        }
        try {
            auto j = nlohmann::json::parse(body);
            (void)j;
        }
        catch (...) {
            console_status::statusf("WARN", "Downloaded offsets are invalid. Keeping current offsets.");
            return false;
        }
        std::ofstream f("offsets.json", std::ios::binary);
        if (!f.is_open()) {
            console_status::statusf("WARN", "Cannot write offsets.json. Check file permissions or close editors using it.");
            return false;
        }
        f << body;
        f.close();

        int rr = OffsetLoader::LoadOffsets();
        if (rr == 1) {
            console_status::statusf("OK", "Offsets updated to %s.", OffsetLoader::GetExpectedVersion().c_str());
            return true;
        }
        console_status::statusf("WARN", "Downloaded offsets could not be loaded. Keeping current offsets.");
        return false;
        };

    probe_and_sync_offsets();

    if (OffsetLoader::WasLoadedFromJson()) {
        std::string expectedVersion = OffsetLoader::GetExpectedVersion();
        if (!robloxVersion.empty() && !expectedVersion.empty() && robloxVersion != expectedVersion) {
            version_mismatch = true;
        }
    }
    else {
        if (!robloxVersion.empty() && robloxVersion != Offsets::ClientVersion) {
            version_mismatch = true;
        }
    }

    if (version_mismatch) {
        console_status::section("Version Check");
        console_status::statusf("WARN", "Offsets do not match this Roblox version.");
        console_status::statusf("INFO", "Roblox version: %s", robloxVersion.empty() ? "unknown" : robloxVersion.c_str());
        console_status::statusf("INFO", "Offsets version: %s", OffsetLoader::GetExpectedVersion().empty() ? Offsets::ClientVersion.c_str() : OffsetLoader::GetExpectedVersion().c_str());
        if (console_status::has_console())
        {
            console_status::action("Press ENTER to continue anyway, or close this window and update offsets.json.");
            std::cin.get();
        }
        else
        {
            console_status::statusf("WARN", "Continuing automatically because the public loader has no console prompt.");
        }
    }

    // Wait for a real experience, not just the Roblox LuaApp shell/main menu.
    console_status::section("Game");
    console_status::statusf("WAIT", "Waiting for an experience to load...");
    startup_game_state_t startup_state{};
    auto last_wait_log = std::chrono::steady_clock::now();
    while (true)
    {
        if (capture_startup_game_state(startup_state))
            break;

        const auto now = std::chrono::steady_clock::now();
        if (now - last_wait_log >= std::chrono::seconds(5))
        {
            if (game::datamodel.address != 0)
            {
                const std::string dm_name = game::read_datamodel_display_name();
                if (startup_state.game_id == 0 && startup_state.place_id == 0)
                {
                    console_status::statusf("WAIT", "Roblox is open%s%s. Join a game/map to continue.",
                        dm_name.empty() ? "" : " in ",
                        dm_name.empty() ? "" : dm_name.c_str());
                }
                else
                {
                    console_status::statusf("WAIT", "Experience detected. Waiting for character and camera to finish loading...");
                }
            }
            else
            {
                console_status::statusf("WAIT", "Waiting for Roblox DataModel...");
            }
            last_wait_log = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    const auto startup_detection = startup_state.detection;
    game::set_active_detection(startup_detection);

    cache::publish_support_report(startup_detection, false);
    console_status::statusf("OK", "Game loaded. Preparing features...");

    // Validate all offsets by reading through them.
    // CORE offsets are required for the cheat to function. Failure on any of them aborts.
    // Non-core offsets only warn — they degrade individual features but don't crash core paths.
    {
        int total = 0, valid = 0, core_failed = 0;
        std::string fails;
        std::string core_fails;
#define VFAIL(name) do { fails += "  - " name "\n"; } while(0)
#define VFAIL_CORE(name) do { fails += "  - Required: " name "\n"; core_fails += "  - " name "\n"; core_failed++; } while(0)
#define VCHECK(name, val) do { total++; if ((val) != 0) valid++; else VFAIL(name); } while(0)
#define VCHECK_CORE(name, val) do { total++; if ((val) != 0) valid++; else VFAIL_CORE(name); } while(0)
#define VREAD(name, base, off) [&]() -> std::uint64_t { total++; if ((base) == 0) { VFAIL(name); return 0; } try { auto v = memory->read<std::uint64_t>((base) + (off)); if (v != 0) { valid++; return v; } } catch (...) {} VFAIL(name); return 0; }()
#define VREAD_CORE(name, base, off) [&]() -> std::uint64_t { total++; if ((base) == 0) { VFAIL_CORE(name); return 0; } try { auto v = memory->read<std::uint64_t>((base) + (off)); if (v != 0) { valid++; return v; } } catch (...) {} VFAIL_CORE(name); return 0; }()
#define VFLOAT(name, base, off, lo, hi) do { total++; try { float v = memory->read<float>((base) + (off)); if (v >= (lo) && v <= (hi)) valid++; else VFAIL(name); } catch (...) { VFAIL(name); } } while(0)

        // CORE — bootstrap pointers
        VCHECK_CORE("FakeDataModel::Pointer", startup_state.fake_datamodel);
        VCHECK_CORE("DataModel", game::datamodel.address);
        VCHECK_CORE("VisualEngine::Pointer", game::visengine.address);
        VCHECK_CORE("Players", game::players.address);

        // CORE — Workspace + Camera are needed by every aim/silent/esp path
        std::uint64_t workspace = game::datamodel.address ? game::datamodel.find_first_child_by_class("Workspace").address : 0;
        VCHECK_CORE("Workspace", workspace);

        // Non-core services (optional feature surfaces)
        std::uint64_t lighting = game::datamodel.address ? game::datamodel.find_first_child_by_class("Lighting").address : 0;
        VCHECK("Lighting", lighting);
        VCHECK("TextChatService", game::datamodel.address ? game::datamodel.find_first_child_by_class("TextChatService").address : 0);
        VCHECK("RunService", game::datamodel.address ? game::datamodel.find_first_child_by_class("RunService").address : 0);
        VCHECK("UserInputService", game::datamodel.address ? game::datamodel.find_first_child_by_class("UserInputService").address : 0);
        VCHECK("CoreGui", game::datamodel.address ? game::datamodel.find_first_child_by_class("CoreGui").address : 0);

        // CORE — Camera + RenderView (screen projection, aimbot)
        std::uint64_t camera = VREAD_CORE("Workspace::CurrentCamera", workspace, Offsets::Workspace::CurrentCamera);
        std::uint64_t world = VREAD("Workspace::World", workspace, Offsets::Workspace::World);

        if (camera) { VREAD_CORE("Camera::CameraSubject", camera, Offsets::Camera::CameraSubject); VFLOAT("Camera::FieldOfView", camera, Offsets::Camera::FieldOfView, 1.0f, 180.0f); }

        // CORE — LocalPlayer / ModelInstance (every feature targeting self or others)
        const bool overkill_workspace_entities = (startup_detection.key == gamesupport::GameKey::Overkill);
        std::uint64_t local_player = 0;
        if (game::players.address) {
            local_player = VREAD_CORE("Player::LocalPlayer", game::players.address, Offsets::Player::LocalPlayer);
            if (local_player) {
                if (overkill_workspace_entities) {
                    VREAD("Player::ModelInstance (optional in Overkill)", local_player, Offsets::Player::ModelInstance);
                }
                else {
                    VREAD_CORE("Player::ModelInstance", local_player, Offsets::Player::ModelInstance);
                }
            }
        }

        // CORE — VisualEngine::RenderView (world_to_screen depends on it)
        std::uint64_t render_view = VREAD_CORE("VisualEngine::RenderView", game::visengine.address, Offsets::VisualEngine::RenderView);
        if (render_view) { VREAD("RenderView::VisualEngine", render_view, Offsets::RenderView::VisualEngine); VREAD("RenderView::DeviceD3D11", render_view, Offsets::RenderView::DeviceD3D11); }

        if (world) { VFLOAT("World::Gravity", world, Offsets::World::Gravity, 0.1f, 10000.0f); VREAD("World::Primitives", world, Offsets::World::Primitives); }

        if (lighting) { VFLOAT("Lighting::Brightness", lighting, Offsets::Lighting::Brightness, 0.0f, 100.0f); VFLOAT("Lighting::ClockTime", lighting, Offsets::Lighting::ClockTime, -1.0f, 48.0f); }

        if (game::datamodel.address) {
            VREAD("DataModel::GameId", game::datamodel.address, Offsets::DataModel::GameId);
            VREAD("DataModel::PlaceId", game::datamodel.address, Offsets::DataModel::PlaceId);
            try {
                rbx::instance_t script_context = game::datamodel.find_first_child_by_class("ScriptContext");
                if (!script_context.address)
                    script_context = game::datamodel.find_first_child("Script Context");
                total++;
                if (script_context.address) valid++;
                else VFAIL("ScriptContext service");
            }
            catch (...) {
                total++;
                VFAIL("ScriptContext service");
            }
        }

        // CORE — Instance walking primitives
        if (game::players.address) {
            try { auto cs = memory->read<std::uint64_t>(game::players.address + Offsets::Instance::ChildrenStart); auto s = memory->read<std::uint64_t>(cs); auto e = memory->read<std::uint64_t>(cs + Offsets::Instance::ChildrenEnd); total++; if (s && e && e > s) valid++; else VFAIL_CORE("Instance::Children"); }
            catch (...) { total++; VFAIL_CORE("Instance::Children"); }
            try { auto np = memory->read<std::uint64_t>(game::players.address + Offsets::Instance::Name); auto n = memory->read_string(np); total++; if (n == "Players") valid++; else VFAIL("Instance::Name"); }
            catch (...) { total++; VFAIL("Instance::Name"); }
            try { auto cd = memory->read<std::uint64_t>(game::players.address + Offsets::Instance::ClassDescriptor); auto cn = memory->read<std::uint64_t>(cd + Offsets::Instance::ClassName); auto c = memory->read_string(cn); total++; if (c == "Players") valid++; else VFAIL_CORE("Instance::ClassName"); }
            catch (...) { total++; VFAIL_CORE("Instance::ClassName"); }
        }

        if (local_player) { try { auto m = rbx::player_t(local_player).get_model_instance(); if (m.address) { auto h = rbx::instance_t(m.address).find_first_child("Humanoid"); if (h.address) { VFLOAT("Humanoid::Health", h.address, Offsets::Humanoid::Health, 0.0f, 1000000.0f); VFLOAT("Humanoid::MaxHealth", h.address, Offsets::Humanoid::MaxHealth, 0.1f, 1000000.0f); VFLOAT("Humanoid::Walkspeed", h.address, Offsets::Humanoid::Walkspeed, 0.0f, 10000.0f); VFLOAT("Humanoid::JumpPower", h.address, Offsets::Humanoid::JumpPower, 0.0f, 10000.0f); } } } catch (...) {} }

        try { auto ts = memory->read<std::uint64_t>(memory->get_module_address() + Offsets::TaskScheduler::Pointer); total++; if (ts) valid++; else VFAIL("TaskScheduler::Pointer"); }
        catch (...) { total++; VFAIL("TaskScheduler::Pointer"); }

#undef VFAIL
#undef VFAIL_CORE
#undef VCHECK
#undef VCHECK_CORE
#undef VREAD
#undef VREAD_CORE
#undef VFLOAT

        char summary[96];
        snprintf(summary, sizeof(summary), "=== %d/%d offsets valid (%d core failures) ===\n", valid, total, core_failed);
        settings::globals::offset_validation_result = "\n=== Offset Validation ===\n" + fails + summary;

        console_status::section("Validation");
        if (core_failed > 0)
        {
            console_status::statusf("ERROR", "Core validation failed (%d/%d offsets valid).", valid, total);
            console_status::statusf("INFO", "The following required offsets failed:");
            printf("%s", core_fails.c_str());
            console_status::action("Update offsets.json or restart after Roblox is fully loaded, then try again.");
            console_status::wait_for_exit_ack();
            return 1;
        }
        else if (!fails.empty())
        {
            console_status::statusf("WARN", "%d/%d offsets validated. Optional checks failed:", valid, total);
            printf("%s", fails.c_str());
            console_status::statusf("INFO", "The program will continue. Some optional features may be unavailable.");
        }
        else
        {
            console_status::statusf("OK", "%d/%d offsets validated. Core systems are ready.", valid, total);
        }
    }

    runtime::spawn(cache::run);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    console_status::section("Storage");
    if (!InitializeStorage())
    {
        console_status::statusf("ERROR", "Could not initialize local storage.");
        console_status::action("Run the program from a writable folder, then try again.");
        console_status::wait_for_exit_ack();
        return 1;
    }
    console_status::statusf("OK", "Local storage is ready.");

    console_status::section("Features");
    console_status::statusf("INFO", "Starting background feature services...");
    runtime::spawn(AutoRescanHandler);
    runtime::spawn(aimbot::run);
    runtime::spawn(silentaim::run);
    runtime::spawn(triggerbot::run_aimbot);
    runtime::spawn(triggerbot::run_silentaim);
    runtime::spawn(rage::hitsounds_detector_thread);
    runtime::spawn(rage::hittracers_detector_thread);
    runtime::spawn(rage::orbit::run);
    runtime::spawn(rage::rapidfire::run);
    runtime::spawn(rage::hitbox_expander::run);
    runtime::spawn(rage::spin360::run);
    rage::desync::initialize();
    runtime::spawn(rage::magicbullet::run);
    runtime::spawn(rage::noclip::run);
    runtime::spawn(rage::hipheight::run);
    runtime::spawn(rage::thirdperson::run);
    runtime::spawn(movement::run);
    runtime::spawn(movement::gravity::run);
    runtime::spawn(lighting::fog::run);
    runtime::spawn(lighting::exposure::run);
    runtime::spawn(lighting::clocktime::run);
    runtime::spawn(lighting::shadows::run);
    runtime::spawn(lighting::skybox::run);
    runtime::spawn(exploits::headless::run);
    runtime::spawn(exploits::korblox::run);
    runtime::spawn(exploits::antiafk::run);
    runtime::spawn(exploits::fpscaps::run);
    runtime::spawn(exploits::freezeplayer::run);
    runtime::spawn(menu::console::run);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    console_status::statusf("OK", "Feature services started.");

    // Autoload config if set
    console_status::section("Config");
    if (!external_config::autoload_config.empty())
    {
        if (config::config_exists(external_config::autoload_config))
        {
            config::load_config(external_config::autoload_config);
            console_status::statusf("OK", "Autoloaded config: %s", external_config::autoload_config.c_str());
        }
        else
        {
            console_status::statusf("WARN", "Autoload config '%s' was not found. Using defaults.", external_config::autoload_config.c_str());
        }
    }
    else
    {
        console_status::statusf("INFO", "No autoload config set. Using current defaults.");
    }

    console_status::section("Overlay");
    if (!render->create_window())
    {
        console_status::statusf("ERROR", "Could not create the overlay window.");
        console_status::action("Make sure the program is not blocked by security software, then restart it.");
        console_status::wait_for_exit_ack();
        return 1;
    }

    if (!render->create_device())
    {
        console_status::statusf("ERROR", "Could not create the DirectX device.");
        console_status::action("Update graphics drivers or restart Roblox, then try again.");
        console_status::wait_for_exit_ack();
        return 1;
    }

    if (!render->create_imgui())
    {
        console_status::statusf("ERROR", "Could not initialize the menu renderer.");
        console_status::action("Restart the program. If this repeats, rebuild and check render dependencies.");
        console_status::wait_for_exit_ack();
        return 1;
    }

    if (!Menu::Initialize(render->detail->window, render->detail->device, render->detail->device_context))
    {
        console_status::statusf("ERROR", "Could not initialize the menu.");
        console_status::action("Restart the program. If this repeats, check menu initialization logs.");
        console_status::wait_for_exit_ack();
        return 1;
    }
    console_status::statusf("OK", "Overlay and menu are ready.");
    console_status::section("Ready");
    console_status::statusf("INFO", "Use your configured menu keybind to open or close the menu.");
    console_status::statusf("INFO", "Press F5 anytime to reload offsets.json.");

    static auto last_pid_check = std::chrono::steady_clock::now();

    auto trigger_panic = []() {
        // Flip every major "enabled" flag off. Each feature thread observes its
        // own disable path and restores world state (HBE, noclip, gravity, walkspeed,
        // lighting captures, etc.) before going idle.
        settings::aimbot::enabled = false;
        settings::aimbot::triggerbot::enabled = false;
        settings::silentaim::enabled = false;
        settings::silentaim::triggerbot::enabled = false;

        settings::rage::rapidfire = false;
        settings::rage::noclip = false;
        settings::rage::hitbox_expander::enabled = false;
        settings::rage::spin360::enabled = false;
        settings::rage::hipheight::enabled = false;
        settings::rage::thirdperson::enabled = false;
        settings::desync::enabled = false;
        settings::magicbullet::enabled = false;

        settings::movement::speedhack::enabled = false;
        settings::movement::jumphack::enabled = false;
        settings::movement::nojumpcooldown::enabled = false;
        settings::movement::flyhack::enabled = false;
        settings::movement::tickrate::enabled = false;
        settings::movement::orbit::enabled = false;
        settings::movement::gravity::enabled = false;

        settings::lighting::fog::enabled = false;
        settings::lighting::shadows::disable = false;
        settings::lighting::clocktime::enabled = false;
        settings::lighting::skybox::enabled = false;
        settings::lighting::exposure::enabled = false;

        settings::exploits::antiafk::enabled = false;
        settings::exploits::freezeplayer::enabled = false;
        settings::cilent::fpscaps::enabled = false;

        console_status::statusf("OK", "Panic key pressed. All toggleable features were disabled.");
        };

    while (runtime::alive())
    {
        // Panic: edge-trigger
        if (settings::menu::panic_keybind != 0 && (GetAsyncKeyState(settings::menu::panic_keybind) & 1))
        {
            trigger_panic();
        }

        // F5: hot-reload offsets.json without restart
        if (GetAsyncKeyState(VK_F5) & 1)
        {
            int rr = OffsetLoader::LoadOffsets();
            if (rr == 1)
                console_status::statusf("OK", "Offsets reloaded from offsets.json (%s).", OffsetLoader::GetExpectedVersion().c_str());
            else if (rr == -1)
                console_status::statusf("WARN", "offsets.json is invalid. Keeping current offsets.");
            else
                console_status::statusf("WARN", "offsets.json is missing or empty. Keeping current offsets.");
        }

        // Check if Roblox is still running
        if (std::chrono::steady_clock::now() - last_pid_check > std::chrono::milliseconds(500))
        {
            DWORD exit_code = 0;
            HANDLE proc = memory->get_process_handle();
            if (proc == nullptr || proc == INVALID_HANDLE_VALUE ||
                (GetExitCodeProcess(proc, &exit_code) && exit_code != STILL_ACTIVE) ||
                memory->find_process_id(BINARY_NAME) == 0)
            {
                console_status::statusf("INFO", "Roblox closed. Shutting down safely...");
                runtime::request_stop();
                break;
            }
            last_pid_check = std::chrono::steady_clock::now();
        }

        render->start_render();

        if (!should_render_ui())
        {
            render->end_render();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        render->render_visuals();

        if (render->running)
        {
            render->render_menu();
        }

        render->end_render();

        if (settings::menu::performance_mode)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    Menu::Shutdown();
    render->running = false;
    runtime::join_all();
    runtime_log::shutdown();

    return 0;
}