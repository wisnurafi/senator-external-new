#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <climits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <gamesupport/BladeBall/blade_ball.h>
#include <settings.h>
#include <cache/cache.h>
#include <cache/bodyparts/bodyparts.h>
#include <game/game.h>
#include <memory/memory.h>
#include <Offsets/Offsets.hpp>
#include <sdk/sdk.h>
#include <imgui/imgui.h>
namespace gamesupport::blade_ball
{
    namespace {
        struct Vec2 {
            float x;
            float y;
        };

        struct Vec3 {
            float x;
            float y;
            float z;
        };

        struct Matrix4 {
            float data[16];
        };

        struct BallInfo {
            uintptr_t primitive = 0;
            unsigned int engagement_id = 0;
            uintptr_t target_character = 0;
            Vec3 position{};
            Vec3 velocity{};
            Vec3 filtered_velocity{};
            Vec3 acceleration{};
            Vec3 predicted_impact_position{};
            bool targeting_local = false;
            float horizontal_distance = 0.0f;
            float vertical_distance = 0.0f;
            float total_distance = 0.0f;
            float speed = 0.0f;
            float filtered_speed = 0.0f;
            float time_to_reach = 0.0f;  // Time in seconds until ball reaches player
            float time_to_intercept = 999.0f;
            float time_to_closest_approach = 999.0f;
            float approach_factor = 0.0f;  // How directly ball is moving toward player (-1 to 1)
            float closest_distance = FLT_MAX;
            float closest_horizontal_distance = FLT_MAX;
            float closest_vertical_distance = FLT_MAX;
            float closing_speed = 0.0f;
            float curve_strength = 0.0f;
            float close_range_factor = 0.0f;
            float parry_distance_threshold = 0.0f;
            float parry_height_threshold = 0.0f;
            float prediction_confidence = 0.0f;
            bool moving_toward_player = false;
            bool predicted_to_intercept = false;
        };

        struct BallTrack {
            Vec3 last_position{};
            Vec3 last_velocity{};
            Vec3 filtered_velocity{};
            Vec3 acceleration{};
            Vec3 last_velocity_direction{};
            float last_approach_factor = 0.0f;
            float last_total_distance = FLT_MAX;
            bool last_targeting_local = false;
            unsigned int engagement_id = 0;
            int away_frame_count = 0;
            std::chrono::steady_clock::time_point last_update{};
            bool valid = false;
        };

        static std::chrono::steady_clock::time_point g_last_parry = std::chrono::steady_clock::now();
        static std::chrono::steady_clock::time_point g_last_in_range = std::chrono::steady_clock::now();
        static std::chrono::steady_clock::time_point g_last_ball_snapshot = std::chrono::steady_clock::time_point{};
        static std::chrono::steady_clock::time_point g_parry_release_at = std::chrono::steady_clock::time_point{};
        static std::chrono::steady_clock::time_point g_last_spam = std::chrono::steady_clock::now();
        static std::chrono::steady_clock::time_point g_next_spam_at = std::chrono::steady_clock::time_point{};
        static uintptr_t g_last_parried_ball = 0;
        static unsigned int g_last_parried_engagement_id = 0;
        static std::unordered_map<uintptr_t, BallTrack> g_ball_tracks;
        static std::vector<BallInfo> g_cached_balls;
        static Vec3 g_cached_local_pos{};
        static bool g_parry_input_down = false;
        static bool g_camera_override_active = false;
        static int g_spam_remaining = 0;

        static Vec3 operator+(const Vec3& lhs, const Vec3& rhs) {
            return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
        }

        static Vec3 operator-(const Vec3& lhs, const Vec3& rhs) {
            return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
        }

        static Vec3 operator*(const Vec3& vec, float scalar) {
            return { vec.x * scalar, vec.y * scalar, vec.z * scalar };
        }

        static Vec3 operator/(const Vec3& vec, float scalar) {
            if (std::fabs(scalar) < 0.0001f) return { 0.0f, 0.0f, 0.0f };
            return { vec.x / scalar, vec.y / scalar, vec.z / scalar };
        }

        static float Dot(const Vec3& lhs, const Vec3& rhs) {
            return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
        }

        static float Length(const Vec3& vec) {
            return std::sqrt(Dot(vec, vec));
        }

        static Vec3 Normalize(const Vec3& vec) {
            float len = Length(vec);
            if (len < 0.0001f) return { 0.0f, 0.0f, 0.0f };
            return { vec.x / len, vec.y / len, vec.z / len };
        }

        static float HorizontalLength(const Vec3& vec) {
            return std::sqrt((vec.x * vec.x) + (vec.z * vec.z));
        }

        static bool IsFiniteFloat(float value) {
            return std::isfinite(value) != 0;
        }

        static bool IsFiniteVec3(const Vec3& vec) {
            return IsFiniteFloat(vec.x) && IsFiniteFloat(vec.y) && IsFiniteFloat(vec.z);
        }

        static Vec3 SanitizeVec3(const Vec3& vec) {
            Vec3 sanitized = vec;
            if (!IsFiniteFloat(sanitized.x)) sanitized.x = 0.0f;
            if (!IsFiniteFloat(sanitized.y)) sanitized.y = 0.0f;
            if (!IsFiniteFloat(sanitized.z)) sanitized.z = 0.0f;
            return sanitized;
        }

        static float ClampFloat(float value, float min_value, float max_value) {
            return std::clamp(value, min_value, max_value);
        }

        static Vec3 LerpVec3(const Vec3& from, const Vec3& to, float alpha) {
            const float t = ClampFloat(alpha, 0.0f, 1.0f);
            return from + (to - from) * t;
        }

        static rbx::vector2int16 MakeViewportFromTarget(const math::vector2& screen_size, const math::vector2& target_screen) {
            const auto clamp_i16 = [](int value) -> int16_t {
                value = std::clamp(value, static_cast<int>(INT16_MIN), static_cast<int>(INT16_MAX));
                return static_cast<int16_t>(value);
            };

            const int vx = static_cast<int>(std::lround(2.0f * (screen_size.x - target_screen.x)));
            const int vy = static_cast<int>(std::lround(2.0f * (screen_size.y - target_screen.y)));

            return rbx::vector2int16{ clamp_i16(vx), clamp_i16(vy) };
        }

        static void ResetCameraViewport() {
            if (!g_camera_override_active || game::camera == 0 || !game::visengine.address) return;

            math::vector2 dims = game::visengine.get_dimensions();
            HWND roblox_window = game::get_roblox_window();
            if (roblox_window) {
                RECT client_rect{};
                if (GetClientRect(roblox_window, &client_rect)) {
                    dims.x = static_cast<float>(client_rect.right - client_rect.left);
                    dims.y = static_cast<float>(client_rect.bottom - client_rect.top);
                }
            }

            rbx::camera_t camera{ game::camera };
            camera.set_viewport({
                static_cast<int16_t>(std::clamp(static_cast<int>(std::lround(dims.x)), static_cast<int>(INT16_MIN), static_cast<int>(INT16_MAX))),
                static_cast<int16_t>(std::clamp(static_cast<int>(std::lround(dims.y)), static_cast<int>(INT16_MIN), static_cast<int>(INT16_MAX)))
            });
            g_camera_override_active = false;
        }

        static Vec3 PredictPosition(const Vec3& position, const Vec3& velocity, const Vec3& acceleration, float time_seconds) {
            const float t = ClampFloat(time_seconds, 0.0f, FLT_MAX);
            return position + (velocity * t) + (acceleration * (0.5f * t * t));
        }

        static void PruneOldBallTracks(const std::chrono::steady_clock::time_point& now) {
            constexpr float stale_seconds = 2.0f;
            for (auto it = g_ball_tracks.begin(); it != g_ball_tracks.end();) {
                const float age = std::chrono::duration<float>(now - it->second.last_update).count();
                if (!it->second.valid || age > stale_seconds) {
                    it = g_ball_tracks.erase(it);
                } else {
                    ++it;
                }
            }
        }

        static void UpdateBallTrack(uintptr_t primitive, const Vec3& position, const Vec3& velocity, BallInfo& info, const std::chrono::steady_clock::time_point& now) {
            BallTrack& track = g_ball_tracks[primitive];
            const Vec3 safe_position = SanitizeVec3(position);
            const Vec3 safe_velocity = SanitizeVec3(velocity);

            if (!track.valid) {
                track.last_position = safe_position;
                track.last_velocity = safe_velocity;
                track.filtered_velocity = safe_velocity;
                track.acceleration = { 0.0f, 0.0f, 0.0f };
                track.last_velocity_direction = Normalize(safe_velocity);
                track.last_update = now;
                track.valid = true;
            } else {
                const float dt = std::chrono::duration<float>(now - track.last_update).count();
                if (dt > 0.0005f) {
                    const Vec3 measured_velocity = SanitizeVec3((safe_position - track.last_position) / dt);
                    const Vec3 blended_velocity = SanitizeVec3(LerpVec3(safe_velocity, measured_velocity, 0.6f));
                    track.filtered_velocity = LerpVec3(track.filtered_velocity, blended_velocity, 0.45f);

                    const Vec3 raw_acceleration = SanitizeVec3((track.filtered_velocity - track.last_velocity) / dt);
                    track.acceleration = LerpVec3(track.acceleration, raw_acceleration, 0.35f);
                    const float acceleration_magnitude = Length(track.acceleration);
                    if (!IsFiniteFloat(acceleration_magnitude)) {
                        track.acceleration = { 0.0f, 0.0f, 0.0f };
                    } else if (acceleration_magnitude > 450.0f) {
                        track.acceleration = Normalize(track.acceleration) * 450.0f;
                    }
                }

                track.filtered_velocity = SanitizeVec3(track.filtered_velocity);
                track.acceleration = SanitizeVec3(track.acceleration);
                track.last_position = safe_position;
                track.last_velocity = track.filtered_velocity;
                track.last_velocity_direction = Normalize(track.filtered_velocity);
                track.last_update = now;
            }

            info.filtered_velocity = track.filtered_velocity;
            info.acceleration = track.acceleration;
            info.filtered_speed = Length(track.filtered_velocity);
        }

        static void AnalyzeBallTrajectory(BallInfo& info, const Vec3& local_pos) {
            info.position = SanitizeVec3(info.position);
            info.velocity = SanitizeVec3(info.velocity);
            info.filtered_velocity = SanitizeVec3(info.filtered_velocity);
            info.acceleration = SanitizeVec3(info.acceleration);
            if (!IsFiniteVec3(local_pos)) {
                return;
            }

            const Vec3 relative_position = info.position - local_pos;
            info.closest_distance = info.total_distance;
            info.closest_horizontal_distance = info.horizontal_distance;
            info.closest_vertical_distance = info.vertical_distance;
            info.predicted_impact_position = info.position;
            info.time_to_reach = 999.0f;
            info.time_to_intercept = 999.0f;
            info.time_to_closest_approach = 999.0f;
            info.predicted_to_intercept = false;
            info.prediction_confidence = 0.0f;
            info.closing_speed = 0.0f;
            info.curve_strength = 0.0f;
            info.close_range_factor = 0.0f;
            info.parry_distance_threshold = settings::blade_ball::parry_distance;
            info.parry_height_threshold = settings::blade_ball::parry_height;
            info.moving_toward_player = false;
            info.approach_factor = 0.0f;

            if (info.filtered_speed < 1.0f || info.total_distance < 0.05f) {
                return;
            }

            const Vec3 toward_local = Normalize(local_pos - info.position);
            const Vec3 velocity_direction = Normalize(info.filtered_velocity);
            info.approach_factor = Dot(velocity_direction, toward_local);
            info.closing_speed = ClampFloat(Dot(info.filtered_velocity, toward_local), 0.0f, FLT_MAX);
            const float lateral_acceleration = Length(info.acceleration - (toward_local * Dot(info.acceleration, toward_local)));
            info.curve_strength = ClampFloat(lateral_acceleration / 220.0f, 0.0f, 1.0f);
            info.close_range_factor = 1.0f - ClampFloat(info.total_distance / 18.0f, 0.0f, 1.0f);
            info.moving_toward_player = info.approach_factor > 0.15f && info.closing_speed > 1.0f;
            const float fast_ball_factor = ClampFloat((info.filtered_speed - 140.0f) / 180.0f, 0.0f, 1.0f);
            const float closing_factor = ClampFloat((info.closing_speed - 45.0f) / 120.0f, 0.0f, 1.0f);
            info.parry_distance_threshold =
                settings::blade_ball::parry_distance +
                (fast_ball_factor * 7.0f) +
                (closing_factor * 4.0f);
            info.parry_height_threshold =
                settings::blade_ball::parry_height +
                (fast_ball_factor * 2.5f) +
                (closing_factor * 1.5f);

            const float speed_factor = ClampFloat(info.filtered_speed / 200.0f, 0.72f, 2.2f);
            const float prediction_horizon = 1.35f * speed_factor;
            constexpr int prediction_steps = 110;
            bool found_intercept = false;

            for (int step = 1; step <= prediction_steps; ++step) {
                const float t = prediction_horizon * (static_cast<float>(step) / static_cast<float>(prediction_steps));
                const Vec3 predicted = PredictPosition(info.position, info.filtered_velocity, info.acceleration, t);
                if (!IsFiniteVec3(predicted)) continue;
                const Vec3 delta = predicted - local_pos;
                const float horizontal = HorizontalLength(delta);
                const float vertical = std::fabs(delta.y);
                const float total = Length(delta);
                if (!IsFiniteFloat(horizontal) || !IsFiniteFloat(vertical) || !IsFiniteFloat(total)) continue;

                if (total < info.closest_distance) {
                    info.closest_distance = total;
                    info.closest_horizontal_distance = horizontal;
                    info.closest_vertical_distance = vertical;
                    info.time_to_closest_approach = t;
                    info.predicted_impact_position = predicted;
                }

                if (!found_intercept &&
                    horizontal <= info.parry_distance_threshold &&
                    vertical <= info.parry_height_threshold) {
                    found_intercept = true;
                    info.predicted_to_intercept = true;
                    info.time_to_intercept = t;
                    info.time_to_reach = t;
                    info.predicted_impact_position = predicted;
                }
            }

            if (info.time_to_closest_approach == 999.0f) {
                const float speed_sq = Dot(info.filtered_velocity, info.filtered_velocity);
                if (speed_sq > 1.0f) {
                    const float closest_t = ClampFloat(-Dot(relative_position, info.filtered_velocity) / speed_sq, 0.0f, prediction_horizon);
                    info.time_to_closest_approach = closest_t;
                    const Vec3 predicted = PredictPosition(info.position, info.filtered_velocity, info.acceleration, closest_t);
                    const Vec3 delta = predicted - local_pos;
                    info.closest_distance = Length(delta);
                    info.closest_horizontal_distance = HorizontalLength(delta);
                    info.closest_vertical_distance = std::fabs(delta.y);
                    info.predicted_impact_position = predicted;
                }
            }

            if (!found_intercept && info.closing_speed > 1.0f) {
                info.time_to_reach = info.total_distance / info.closing_speed;
            }

            const float intercept_score =
                (info.predicted_to_intercept ? 0.65f : 0.0f) +
                ClampFloat(info.approach_factor, 0.0f, 1.0f) * 0.18f +
                ClampFloat(info.closing_speed / 220.0f, 0.0f, 1.0f) * 0.12f +
                info.curve_strength * 0.05f;
            info.prediction_confidence = ClampFloat(intercept_score, 0.0f, 1.0f);
        }

        static void UpdateBallEngagement(BallInfo& info) {
            auto track_it = g_ball_tracks.find(info.primitive);
            if (track_it == g_ball_tracks.end()) return;

            BallTrack& track = track_it->second;
            const bool newly_targeted = info.targeting_local && !track.last_targeting_local;
            const bool bounced_back_toward_local =
                info.moving_toward_player &&
                track.last_approach_factor < -0.20f &&
                track.away_frame_count >= 1;
            const bool regained_after_away =
                info.targeting_local &&
                track.away_frame_count >= 2 &&
                info.approach_factor > 0.20f &&
                info.predicted_to_intercept;

            const bool moving_away_now =
                !info.targeting_local ||
                info.approach_factor < -0.05f ||
                (!info.predicted_to_intercept && info.total_distance > track.last_total_distance + 0.5f);

            if (moving_away_now) {
                track.away_frame_count = (std::min)(track.away_frame_count + 1, 5);
            } else {
                track.away_frame_count = 0;
            }

            if ((newly_targeted || bounced_back_toward_local || regained_after_away) &&
                (info.predicted_to_intercept || info.moving_toward_player)) {
                ++track.engagement_id;
            } else if (track.engagement_id == 0 &&
                       info.targeting_local &&
                       (info.predicted_to_intercept || info.moving_toward_player)) {
                track.engagement_id = 1;
            }

            info.engagement_id = track.engagement_id;
            track.last_targeting_local = info.targeting_local;
            track.last_approach_factor = info.approach_factor;
            track.last_total_distance = info.total_distance;
        }

                static bool IsValidAddress(std::uint64_t address) {
            return address > 0x10000 && address < 0x00007FFFFFFFFFFFULL && address != 0xFFFFFFFFFFFFFFFFULL;
        }

        static bool ReadRaw(std::uint64_t address, void* buffer, std::size_t size) {
            if (!memory || !memory->get_process_handle() || !IsValidAddress(address) || buffer == nullptr || size == 0) return false;
            ULONG bytes_read = 0;
            Luck_ReadVirtualMemory(memory->get_process_handle(), reinterpret_cast<void*>(address), buffer, static_cast<ULONG>(size), &bytes_read);
            return bytes_read == static_cast<ULONG>(size);
        }

        static bool ReadVec2(uint64_t address, Vec2& out) {
            return ReadRaw(address, &out, sizeof(out));
        }

        static bool ReadVec3(uint64_t address, Vec3& out) {
            return ReadRaw(address, &out, sizeof(out));
        }

        static bool ReadMatrix(uint64_t address, Matrix4& out) {
            return ReadRaw(address, &out, sizeof(out));
        }

        [[maybe_unused]] static bool IsBallTargetingLocalByAttribute(const rbx::instance_t& ball) {
            if (ball.address == 0) return false;

            // Get local player character
            rbx::instance_t dm = game::datamodel;
            if (dm.address == 0) return false;

            rbx::instance_t players = game::players;
            if (players.address == 0) return false;

            rbx::player_t local_player{ memory->read<std::uint64_t>(players.address + Offsets::Player::LocalPlayer) };
            if (local_player.address == 0) return false;

            rbx::instance_t character = local_player.get_model_instance();
            if (character.address == 0) return false;

            // Check if ball has a "Target" child pointing to our character
            rbx::instance_t target = ball.find_first_child("Target");
            if (target.address != 0) {
                // Read the ObjectValue - it should point to our character
                uintptr_t target_value = memory->read<uintptr_t>(target.address + Offsets::Misc::Value);
                if (target_value == character.address) {
                    return true;
                }
            }

            // Also check if our character has a Highlight (alternative method)
            rbx::instance_t char_highlight = character.find_first_child("Highlight");
            if (char_highlight.address != 0) {
                return true;
            }

            return false;
        }

        static uintptr_t GetBallTargetCharacterAddress(const rbx::instance_t& ball, uintptr_t local_character_address, bool& out_targeting_local) {
            out_targeting_local = false;
            if (ball.address == 0) return 0;

            rbx::instance_t target = ball.find_first_child("Target");
            if (target.address != 0) {
                uintptr_t target_value = memory->read<uintptr_t>(target.address + Offsets::Misc::Value);
                if (IsValidAddress(target_value)) {
                    out_targeting_local = (target_value == local_character_address);
                    return target_value;
                }
            }

            if (IsValidAddress(local_character_address)) {
                rbx::instance_t local_character{ local_character_address };
                rbx::instance_t char_highlight = local_character.find_first_child("Highlight");
                if (char_highlight.address != 0) {
                    out_targeting_local = true;
                    return local_character_address;
                }
            }

            return 0;
        }
        static bool GetCamera(Matrix4& view, Vec2& viewport)
        {
            if (!IsValidAddress(game::visengine.address)) return false;
            if (!ReadMatrix(game::visengine.address + Offsets::VisualEngine::ViewMatrix, view)) return false;
            if (!ReadVec2(game::visengine.address + Offsets::VisualEngine::Dimensions, viewport)) return false;
            if (viewport.x <= 0.0f || viewport.y <= 0.0f) return false;
            return true;
        }

        static bool WorldToScreen(const Vec3& world, Vec2& out, const Matrix4& view, const Vec2& viewport) {
            math::matrix4 view_matrix{};
            std::memcpy(view_matrix.data(), view.data, sizeof(view.data));

            math::vector2 screen{};
            const bool ok = game::visengine.world_to_screen(
                { world.x, world.y, world.z },
                screen,
                { viewport.x, viewport.y },
                view_matrix);
            if (!ok || !IsFiniteFloat(screen.x) || !IsFiniteFloat(screen.y)) return false;

            out = { screen.x, screen.y };
            return true;
        }

        static bool IsNumericName(const std::string& name) {
            return !name.empty() && std::all_of(name.begin(), name.end(), [](unsigned char c) {
                return std::isdigit(c) != 0;
            });
        }

        static uintptr_t GetPrimitiveFromInstance(const rbx::instance_t& inst) {
            if (inst.address == 0) return 0;

            uintptr_t primitive = memory->read<uintptr_t>(inst.address + Offsets::BasePart::Primitive);
            if (IsValidAddress(primitive)) return primitive;

            for (const rbx::instance_t& child : inst.get_children()) {
                if (child.address == 0) continue;
                primitive = memory->read<uintptr_t>(child.address + Offsets::BasePart::Primitive);
                if (IsValidAddress(primitive)) return primitive;
            }

            return 0;
        }

        static std::vector<rbx::instance_t> GetBladeBallCandidates() {
            std::vector<rbx::instance_t> balls;
            balls.reserve(8);

            rbx::instance_t dm = game::datamodel;
            if (dm.address == 0) return balls;

            rbx::instance_t workspace = game::workspace;
            if (workspace.address == 0) return balls;

            rbx::instance_t balls_folder = workspace.find_first_child("Balls");
            if (balls_folder.address != 0) {
                for (const rbx::instance_t& child : balls_folder.get_children()) {
                    if (child.address != 0) {
                        balls.push_back(child);
                    }
                }
            }

            if (!balls.empty()) return balls;

            for (const rbx::instance_t& child : workspace.get_children()) {
                if (child.address == 0) continue;
                if (IsNumericName(child.get_name())) {
                    balls.push_back(child);
                }
            }

            return balls;
        }

        static bool GetLocalPlayerAndCharacter(rbx::instance_t& out_player, rbx::instance_t& out_character) {
            out_player = {};
            out_character = {};

            rbx::instance_t dm = game::datamodel;
            if (dm.address == 0) return false;

            rbx::instance_t players = game::players;
            if (players.address == 0) return false;

            rbx::player_t local_player{ memory->read<std::uint64_t>(players.address + Offsets::Player::LocalPlayer) };
            if (local_player.address == 0) return false;

            out_player = local_player;
            out_character = local_player.get_model_instance();
            return out_character.address != 0;
        }

        static bool ReadPartPosition(const rbx::instance_t& part, Vec3& out_pos)
        {
            if (part.address == 0) return false;
            rbx::primitive_t primitive = rbx::part_t(part.address).get_primitive();
            if (!IsValidAddress(primitive.address)) return false;

            const math::vector3 pos = primitive.get_position();
            out_pos = { pos.x, pos.y, pos.z };
            return IsFiniteVec3(out_pos);
        }

        static bool ReadLocalPosition(Vec3& out_pos)
        {
            if (cache::cached_local_player.instance.address != 0)
            {
                auto it = cache::cached_local_player.parts.find("HumanoidRootPart");
                if (it != cache::cached_local_player.parts.end() && ReadPartPosition(it->second, out_pos)) return true;

                it = cache::cached_local_player.parts.find("UpperTorso");
                if (it != cache::cached_local_player.parts.end() && ReadPartPosition(it->second, out_pos)) return true;

                it = cache::cached_local_player.parts.find("Torso");
                if (it != cache::cached_local_player.parts.end() && ReadPartPosition(it->second, out_pos)) return true;
            }

            rbx::instance_t character = game::local_character;
            if (character.address == 0 && game::players.address != 0)
            {
                rbx::player_t local_player{ memory->read<std::uint64_t>(game::players.address + Offsets::Player::LocalPlayer) };
                if (local_player.address != 0)
                    character = local_player.get_model_instance();
            }

            if (character.address == 0) return false;

            static const char* root_candidates[] = { "HumanoidRootPart", "UpperTorso", "Torso", "Head" };
            for (const char* name : root_candidates)
            {
                rbx::instance_t part = character.find_first_child(name);
                if (ReadPartPosition(part, out_pos)) return true;
            }

            return false;
        }

        static bool GetCachedEntityPosition(const cache::entity_t& entity, Vec3& out_pos) {
            math::vector3 pos{};
            if (bodyparts::get_part_position(entity, "HumanoidRootPart", pos) ||
                bodyparts::get_part_position(entity, "Head", pos)) {
                out_pos = { pos.x, pos.y, pos.z };
                return IsFiniteVec3(out_pos);
            }

            static const char* fallback_parts[] = { "UpperTorso", "Torso", "LowerTorso" };
            for (const char* part_name : fallback_parts) {
                auto it = entity.parts.find(part_name);
                if (it == entity.parts.end() || it->second.address == 0) continue;

                Vec3 part_pos{};
                if (ReadPartPosition(it->second, part_pos)) {
                    out_pos = part_pos;
                    return true;
                }
            }

            return false;
        }

        static bool FindClosestPlayerToBall(const Vec3& ball_pos, Vec3& out_pos) {
            std::vector<cache::entity_t> players_snapshot;
            cache::entity_t local_snapshot{};
            {
                std::lock_guard<std::mutex> lock(cache::mtx);
                players_snapshot = cache::cached_players;
                local_snapshot = cache::cached_local_player;
            }

            float best_distance = FLT_MAX;
            bool found = false;

            for (const cache::entity_t& player : players_snapshot) {
                if (player.instance.address == 0 || player.instance.address == local_snapshot.instance.address) continue;
                if (player.knocked || player.health <= 0.0f) continue;
                if (settings::rage::is_whitelisted(player.name)) continue;

                Vec3 player_pos{};
                if (!GetCachedEntityPosition(player, player_pos)) continue;

                const float distance = Length(player_pos - ball_pos);
                if (distance < best_distance) {
                    best_distance = distance;
                    out_pos = player_pos;
                    found = true;
                }
            }

            return found;
        }

        static const BallInfo* SelectCameraBall(const std::vector<BallInfo>& balls) {
            const BallInfo* best_ball = nullptr;
            float best_score = -FLT_MAX;

            for (const BallInfo& ball : balls) {
                const float score =
                    (ball.targeting_local ? 500.0f : 0.0f) +
                    (ball.predicted_to_intercept ? 220.0f : 0.0f) +
                    (ball.prediction_confidence * 100.0f) +
                    (ball.closing_speed * 0.25f) -
                    (ball.total_distance * 2.0f);

                if (score > best_score) {
                    best_score = score;
                    best_ball = &ball;
                }
            }

            return best_ball;
        }

        static void ApplyCameraTarget(const std::vector<BallInfo>& balls) {
            if ((!settings::blade_ball::look_at_ball && !settings::blade_ball::target_closest_player) ||
                balls.empty() ||
                game::camera == 0 ||
                !game::visengine.address) {
                return;
            }

            const BallInfo* ball = SelectCameraBall(balls);
            if (ball == nullptr) return;

            Vec3 target_pos = ball->position;
            if (settings::blade_ball::target_closest_player) {
                Vec3 closest_player_pos{};
                if (FindClosestPlayerToBall(ball->position, closest_player_pos)) {
                    target_pos = closest_player_pos;
                } else if (!settings::blade_ball::look_at_ball) {
                    return;
                }
            }

            const math::matrix4 view = game::visengine.get_viewmatrix();
            math::vector2 dims = game::visengine.get_dimensions();
            HWND roblox_window = game::get_roblox_window();
            if (roblox_window) {
                RECT client_rect{};
                if (GetClientRect(roblox_window, &client_rect)) {
                    dims.x = static_cast<float>(client_rect.right - client_rect.left);
                    dims.y = static_cast<float>(client_rect.bottom - client_rect.top);
                }
            }

            math::vector2 screen_pos{};
            if (!game::visengine.world_to_screen({ target_pos.x, target_pos.y, target_pos.z }, screen_pos, dims, view)) return;

            screen_pos.x -= game::window_offset_x;
            screen_pos.y -= game::window_offset_y;

            rbx::camera_t camera{ game::camera };
            camera.set_viewport(MakeViewportFromTarget(dims, screen_pos));
            g_camera_override_active = true;
        }

        static bool GatherBallInfo(std::vector<BallInfo>& out_balls, Vec3& local_pos) {
            out_balls.clear();
            if (!ReadLocalPosition(local_pos)) return false;
            if (!IsFiniteVec3(local_pos)) return false;

            rbx::instance_t local_player{};
            rbx::instance_t local_character{};
            uintptr_t local_character_address = 0;
            if (GetLocalPlayerAndCharacter(local_player, local_character)) {
                local_character_address = local_character.address;
            }

            const auto now = std::chrono::steady_clock::now();
            PruneOldBallTracks(now);

            std::vector<rbx::instance_t> candidates = GetBladeBallCandidates();

            for (const rbx::instance_t& ball : candidates) {
                uintptr_t primitive = GetPrimitiveFromInstance(ball);
                if (!IsValidAddress(primitive)) continue;

                Vec3 ball_pos{};
                if (!ReadVec3(primitive + Offsets::Primitive::Position, ball_pos)) continue;
                ball_pos = SanitizeVec3(ball_pos);
                if (ball_pos.x == 0.0f && ball_pos.y == 0.0f && ball_pos.z == 0.0f) continue;

                Vec3 ball_vel{};
                ReadVec3(primitive + Offsets::Primitive::AssemblyLinearVelocity, ball_vel);
                ball_vel = SanitizeVec3(ball_vel);

                float dx = ball_pos.x - local_pos.x;
                float dy = ball_pos.y - local_pos.y;
                float dz = ball_pos.z - local_pos.z;

                BallInfo info{};
                info.primitive = primitive;
                info.position = ball_pos;
                info.velocity = ball_vel;
                info.horizontal_distance = std::sqrt(dx * dx + dz * dz);
                info.vertical_distance = std::fabs(dy);
                info.total_distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                info.speed = std::sqrt(ball_vel.x * ball_vel.x + ball_vel.y * ball_vel.y + ball_vel.z * ball_vel.z);
                info.target_character = GetBallTargetCharacterAddress(ball, local_character_address, info.targeting_local);
                UpdateBallTrack(primitive, ball_pos, ball_vel, info, now);
                AnalyzeBallTrajectory(info, local_pos);
                UpdateBallEngagement(info);

                out_balls.push_back(info);
            }

            return !out_balls.empty();
        }

        static void UpdateParryInputState(const std::chrono::steady_clock::time_point& now) {
            if (g_parry_input_down && now >= g_parry_release_at) {
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                g_parry_input_down = false;
                g_parry_release_at = std::chrono::steady_clock::time_point{};
            }
        }

        static void SendParryInput(const std::chrono::steady_clock::time_point& now) {
            UpdateParryInputState(now);
            if (g_parry_input_down) {
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                g_parry_input_down = false;
            }
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            g_parry_input_down = true;
            g_parry_release_at = now + std::chrono::milliseconds(50);
        }

        static void StartSpamBurst(const std::chrono::steady_clock::time_point& now) {
            const int count = std::clamp(settings::blade_ball::spam_count, 1, 12);
            g_spam_remaining = count;
            g_next_spam_at = now;
            g_last_spam = now;
        }

        static void UpdateSpamBurst(const std::chrono::steady_clock::time_point& now) {
            if (!settings::blade_ball::auto_spam) {
                g_spam_remaining = 0;
                g_next_spam_at = std::chrono::steady_clock::time_point{};
                return;
            }
            if (g_spam_remaining <= 0) return;
            if (g_next_spam_at.time_since_epoch().count() != 0 && now < g_next_spam_at) return;

            SendParryInput(now);
            --g_spam_remaining;
            g_next_spam_at = now + std::chrono::milliseconds(65);
        }

        static bool ShouldSpamParry(const BallInfo& ball, float seconds_since_last_spam) {
            if (!settings::blade_ball::auto_spam) return false;
            if (!ball.targeting_local) return false;
            if (!ball.moving_toward_player && !ball.predicted_to_intercept) return false;
            if (seconds_since_last_spam < 0.16f || g_spam_remaining > 0) return false;

            const float sensitivity = ClampFloat(settings::blade_ball::spam_sensitivity, 0.0f, 1.0f);
            const float time_window = 0.045f + (sensitivity * 0.155f);
            const float distance_scale = 0.65f + (sensitivity * 0.85f);
            const float close_horizontal = ball.parry_distance_threshold * distance_scale;
            const float close_vertical = ball.parry_height_threshold * (1.0f + sensitivity * 0.45f);
            const float urgency =
                (ball.predicted_to_intercept ? 1.0f : 0.0f) +
                ClampFloat(ball.prediction_confidence, 0.0f, 1.0f) +
                ClampFloat(ball.closing_speed / 160.0f, 0.0f, 1.0f) +
                ClampFloat(ball.filtered_speed / 260.0f, 0.0f, 1.0f);

            const bool near_now =
                ball.horizontal_distance <= close_horizontal &&
                ball.vertical_distance <= close_vertical;
            const bool soon =
                ball.predicted_to_intercept &&
                ball.time_to_intercept <= time_window &&
                ball.prediction_confidence >= (0.34f - sensitivity * 0.12f);
            const bool panic_close =
                ball.closest_horizontal_distance <= close_horizontal * 0.80f &&
                ball.closest_vertical_distance <= close_vertical &&
                urgency >= (1.85f - sensitivity * 0.45f);

            return near_now || soon || panic_close;
        }

        static bool ShouldHoldForCurve(const BallInfo& ball) {
            if (!settings::blade_ball::anti_curve) return false;
            if (!ball.predicted_to_intercept) return false;

            const float curve = ClampFloat(ball.curve_strength, 0.0f, 1.0f);
            if (curve < 0.28f) return false;

            const float close_factor = ClampFloat(ball.close_range_factor, 0.0f, 1.0f);
            const bool already_close =
                ball.horizontal_distance <= ball.parry_distance_threshold * 0.72f &&
                ball.vertical_distance <= ball.parry_height_threshold;
            const bool stable_enough =
                ball.approach_factor > 0.62f &&
                ball.prediction_confidence > 0.58f &&
                ball.time_to_intercept <= 0.055f;

            if (already_close || stable_enough || close_factor > 0.82f) return false;

            const float curve_delay_window = 0.075f + (curve * 0.065f);
            return ball.time_to_intercept > curve_delay_window;
        }

    }

    void run() {
        const auto now = std::chrono::steady_clock::now();
        UpdateParryInputState(now);
        UpdateSpamBurst(now);

        const bool camera_feature_enabled =
            settings::blade_ball::look_at_ball ||
            settings::blade_ball::target_closest_player;
        if (!camera_feature_enabled) ResetCameraViewport();
        if (!settings::blade_ball::auto_parry && !settings::blade_ball::auto_spam && !camera_feature_enabled) return;

        Vec3 local_pos{};
        std::vector<BallInfo> balls;
        if (!GatherBallInfo(balls, local_pos)) {
            ResetCameraViewport();
            const float seconds_since_seen = std::chrono::duration<float>(now - g_last_in_range).count();
            if (seconds_since_seen > 0.20f) {
                g_last_parried_ball = 0;
                g_last_parried_engagement_id = 0;
            }
            return;
        }

        g_cached_balls = balls;
        g_cached_local_pos = local_pos;
        g_last_ball_snapshot = now;

        if (camera_feature_enabled) {
            ApplyCameraTarget(balls);
        }

        if (!settings::blade_ball::auto_parry && !settings::blade_ball::auto_spam) return;

        const float seconds_since_last_parry = std::chrono::duration<float>(now - g_last_parry).count();
        const float seconds_since_last_spam = std::chrono::duration<float>(now - g_last_spam).count();
        constexpr float rearm_grace_period = 0.18f;

        BallInfo* most_dangerous = nullptr;
        float best_score = -FLT_MAX;

            for (size_t i = 0; i < balls.size(); i++) {
                BallInfo& ball = balls[i];

                if (!ball.targeting_local) continue;
                if (!ball.moving_toward_player && !ball.predicted_to_intercept) continue;

                const bool currently_in_range =
                    ball.horizontal_distance <= ball.parry_distance_threshold &&
                    ball.vertical_distance <= ball.parry_height_threshold;
                const bool predicted_threat =
                    ball.predicted_to_intercept ||
                    (ball.closest_horizontal_distance <= ball.parry_distance_threshold * 0.85f &&
                     ball.closest_vertical_distance <= ball.parry_height_threshold);

                if (!currently_in_range && !predicted_threat) continue;

            const float time_score = ball.predicted_to_intercept ? ball.time_to_intercept : ball.time_to_closest_approach;
            const float danger_score =
                (ball.predicted_to_intercept ? 140.0f : 0.0f) +
                (ball.prediction_confidence * 80.0f) +
                (ball.closing_speed * 0.20f) +
                (ball.filtered_speed * 0.08f) -
                (time_score * 55.0f) -
                (ball.closest_distance * 1.5f);

            if (danger_score > best_score) {
                best_score = danger_score;
                most_dangerous = &ball;
            }
        }

        if (most_dangerous != nullptr) {
            g_last_in_range = now;
            if (ShouldSpamParry(*most_dangerous, seconds_since_last_spam)) {
                StartSpamBurst(now);
                UpdateSpamBurst(now);
            }

            if (!settings::blade_ball::auto_parry) return;

            const float intercept_time =
                most_dangerous->predicted_to_intercept
                ? most_dangerous->time_to_intercept
                : most_dangerous->time_to_reach;

            float reaction_buffer = 0.038f;
            reaction_buffer += ClampFloat(most_dangerous->filtered_speed / 280.0f, 0.0f, 1.0f) * 0.022f;
            reaction_buffer += ClampFloat(most_dangerous->filtered_speed / 180.0f, 0.0f, 1.0f) * 0.018f;
            reaction_buffer += ClampFloat(Length(most_dangerous->acceleration) / 320.0f, 0.0f, 1.0f) * 0.010f;
            reaction_buffer += most_dangerous->curve_strength * 0.012f;
            reaction_buffer += (1.0f - ClampFloat(most_dangerous->approach_factor, 0.0f, 1.0f)) * 0.010f;
            reaction_buffer -= ClampFloat(most_dangerous->prediction_confidence, 0.0f, 1.0f) * 0.008f;
            reaction_buffer -= ClampFloat((120.0f - most_dangerous->filtered_speed) / 120.0f, 0.0f, 1.0f) * 0.016f;
            reaction_buffer = ClampFloat(reaction_buffer, 0.034f, 0.090f);
            constexpr float single_fire_cooldown = 0.20f;
            const float min_confidence =
                (most_dangerous->filtered_speed > 180.0f && most_dangerous->closing_speed > 60.0f)
                    ? 0.28f
                    : ((most_dangerous->filtered_speed < 120.0f && most_dangerous->closing_speed < 45.0f)
                        ? 0.52f
                        : 0.45f);
            const float min_intercept_time =
                ClampFloat((120.0f - most_dangerous->filtered_speed) / 120.0f, 0.0f, 1.0f) * 0.028f;
            const bool same_ball_as_last_parry = g_last_parried_ball == most_dangerous->primitive;
            const bool same_engagement_as_last_parry = g_last_parried_engagement_id == most_dangerous->engagement_id;
            const bool ball_has_passed =
                !most_dangerous->moving_toward_player ||
                most_dangerous->approach_factor < -0.10f ||
                (seconds_since_last_parry > 0.35f && !most_dangerous->predicted_to_intercept);
            const bool should_rearm_for_new_pass =
                same_ball_as_last_parry &&
                same_engagement_as_last_parry &&
                ball_has_passed;

            if (should_rearm_for_new_pass) {
                g_last_parried_ball = 0;
                g_last_parried_engagement_id = 0;
            }

            const bool should_parry =
                most_dangerous->predicted_to_intercept &&
                !ShouldHoldForCurve(*most_dangerous) &&
                intercept_time <= reaction_buffer &&
                intercept_time >= min_intercept_time &&
                most_dangerous->prediction_confidence >= min_confidence &&
                most_dangerous->closing_speed >= 12.0f &&
                seconds_since_last_parry >= single_fire_cooldown &&
                !(same_ball_as_last_parry && same_engagement_as_last_parry);

            if (should_parry) {
                SendParryInput(now);
                g_last_parry = now;
                g_last_parried_ball = most_dangerous->primitive;
                g_last_parried_engagement_id = most_dangerous->engagement_id;
                return;
            }
        } else {
            const float seconds_since_seen = std::chrono::duration<float>(now - g_last_in_range).count();
            if (seconds_since_seen > rearm_grace_period) {
                g_last_parried_ball = 0;
                g_last_parried_engagement_id = 0;
            }
        }
    }

    void render() {
        if (!settings::blade_ball::ball_esp) return;

        const auto now = std::chrono::steady_clock::now();

        Matrix4 view{};
        Vec2 viewport{};
        if (!GetCamera(view, viewport)) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        Vec3 local_pos = g_cached_local_pos;
        const std::vector<BallInfo>* balls = &g_cached_balls;
        const float snapshot_age = (g_last_ball_snapshot.time_since_epoch().count() == 0)
            ? 999.0f
            : std::chrono::duration<float>(now - g_last_ball_snapshot).count();

        std::vector<BallInfo> fresh_balls;
        if (snapshot_age > 0.03f || balls->empty() || !IsFiniteVec3(local_pos)) {
            if (!GatherBallInfo(fresh_balls, local_pos)) return;
            balls = &fresh_balls;
            g_cached_balls = fresh_balls;
            g_cached_local_pos = local_pos;
            g_last_ball_snapshot = now;
        }

        if (settings::blade_ball::ball_esp) {
            Vec2 local_screen{};
            if (!WorldToScreen(local_pos, local_screen, view, viewport)) return;

            const BallInfo* display_ball = nullptr;
            float best_display_score = -FLT_MAX;
            for (const BallInfo& ball : *balls) {
                if (!ball.targeting_local) continue;
                const float display_score =
                    (ball.predicted_to_intercept ? 100.0f : 0.0f) +
                    (ball.prediction_confidence * 60.0f) +
                    (ball.closing_speed * 0.15f) -
                    (ball.time_to_intercept * 45.0f);

                if (display_score > best_display_score) {
                    best_display_score = display_score;
                    display_ball = &ball;
                }
            }
            const float display_horizontal_envelope =
                (display_ball != nullptr)
                ? display_ball->parry_distance_threshold
                : 12.0f;

            constexpr int segments = 32;
            ImVec2 ring_points[segments];
            int ring_count = 0;

            for (int i = 0; i < segments; ++i) {
                float angle = (2.0f * 3.14159265f * (float)i) / (float)segments;
                Vec3 point{
                    local_pos.x + std::cos(angle) * display_horizontal_envelope,
                    local_pos.y,
                    local_pos.z + std::sin(angle) * display_horizontal_envelope
                };

                Vec2 screen{};
                if (WorldToScreen(point, screen, view, viewport)) {
                    ring_points[ring_count++] = ImVec2(screen.x, screen.y);
                }
            }

            if (ring_count >= 3) {
                draw->AddPolyline(ring_points, ring_count, IM_COL32(0, 255, 180, 180), ImDrawFlags_Closed, 2.0f);
            }

            for (const BallInfo& ball : *balls) {
                Vec2 ball_screen{};
                if (!WorldToScreen(ball.position, ball_screen, view, viewport)) continue;

                bool in_range =
                    ball.horizontal_distance <= ball.parry_distance_threshold &&
                    ball.vertical_distance <= ball.parry_height_threshold;
                ImU32 color = IM_COL32(255, 255, 0, 170);
                if (ball.targeting_local) {
                    color = IM_COL32(255, 64, 64, 255);
                } else if (in_range) {
                    color = IM_COL32(255, 180, 0, 220);
                }

                draw->AddLine(ImVec2(local_screen.x, local_screen.y), ImVec2(ball_screen.x, ball_screen.y), color, 2.0f);
                draw->AddCircleFilled(ImVec2(ball_screen.x, ball_screen.y), 4.0f, color);

                if (ball.targeting_local) {
                    if (ball.predicted_to_intercept) {
                        Vec2 predicted_screen{};
                        if (WorldToScreen(ball.predicted_impact_position, predicted_screen, view, viewport)) {
                            draw->AddLine(ImVec2(ball_screen.x, ball_screen.y), ImVec2(predicted_screen.x, predicted_screen.y), IM_COL32(0, 255, 120, 180), 1.5f);
                            draw->AddCircle(ImVec2(predicted_screen.x, predicted_screen.y), 12.0f, IM_COL32(255, 255, 255, 180), 24, 1.5f);
                        }
                    }
                }
            }
        }

    }
}