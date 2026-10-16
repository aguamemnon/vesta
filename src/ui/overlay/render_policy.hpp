#pragma once
#include <algorithm>
#include <cstdint>

namespace ui
{
inline constexpr int overlay_gpu_priority = -2;
struct feature_plan
{
    bool sample_players{};
    bool gpu_effects{};
};
[[nodiscard]] constexpr feature_plan plan_features(bool attached, bool player_esp, bool chams, bool hardware_device,
                                                   bool use_gpu) noexcept
{
    const bool gpu = hardware_device && use_gpu;
    return {attached && (player_esp || (gpu && chams)), gpu};
}
// Compute an overlay-only frame budget. The game is never frame-limited.
[[nodiscard]] constexpr std::uint32_t overlay_frame_rate(std::uint32_t configured_rate,
                                                         std::uint32_t display_refresh,
                                                         bool performance_mode) noexcept
{
    if (!performance_mode)
        return configured_rate;
    const auto limit = display_refresh ? std::min(display_refresh, 90u) : 90u;
    return configured_rate ? std::min(configured_rate, limit) : limit;
}

static_assert(overlay_frame_rate(240, 60, true) == 60);
static_assert(overlay_frame_rate(240, 144, true) == 90);
static_assert(overlay_frame_rate(45, 144, true) == 45);
static_assert(overlay_frame_rate(0, 0, true) == 90);
static_assert(overlay_frame_rate(0, 144, false) == 0);
static_assert(overlay_frame_rate(240, 60, false) == 240);

[[nodiscard]] constexpr std::uint32_t pose_sample_limit(bool limited, std::uint32_t fps,
                                                        bool performance_mode = false) noexcept
{
    // Pose snapshots involve remote process/bone reads ON the presentation
    // thread. Previously a capped 240 FPS overlay sampled all players on
    // every frame. A 60 Hz ceiling prevents these reads from monopolizing
    // rendering without changing the update loop or game input.
    if (performance_mode)
        return 60u;
    return limited && fps > 0 && fps <= 240 ? 0u : 240u;
}
} // namespace ui
