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
// Performance Mode tracks the actual display rate, without old 90 Hz cap.
// Explicitly configured FPS limits remain respected; the game's frame
// pacing is never modified. Unknown refresh falls back to a 165 Hz target.
[[nodiscard]] constexpr std::uint32_t overlay_frame_rate(std::uint32_t configured_rate,
                                                         std::uint32_t display_refresh,
                                                         bool performance_mode) noexcept
{
    if (!performance_mode)
        return configured_rate;
    const auto target = display_refresh ? display_refresh : 165u;
    return configured_rate ? std::min(configured_rate, target) : target;
}

static_assert(overlay_frame_rate(240, 60, true) == 60);
static_assert(overlay_frame_rate(240, 144, true) == 144);
static_assert(overlay_frame_rate(240, 165, true) == 165);
static_assert(overlay_frame_rate(240, 240, true) == 240);
static_assert(overlay_frame_rate(45, 144, true) == 45);
static_assert(overlay_frame_rate(0, 0, true) == 165);
static_assert(overlay_frame_rate(0, 144, false) == 0);
static_assert(overlay_frame_rate(240, 60, false) == 240);

// Reads now take place on an independent worker, so they no longer block
// ImGui or DXGI. Match the configured refresh target in Performance Mode.
// Outside Performance Mode, follow the configured FPS budget up to 240 Hz.
[[nodiscard]] constexpr std::uint32_t pose_sample_limit(bool limited, std::uint32_t fps,
                                                        bool performance_mode = false,
                                                        std::uint32_t display_refresh = 0) noexcept
{
    if (performance_mode)
        return overlay_frame_rate(limited ? fps : 0u, display_refresh, true);
    return limited && fps > 0 ? std::min(fps, 240u) : 240u;
}
static_assert(pose_sample_limit(true, 240, true, 165) == 165);
static_assert(pose_sample_limit(true, 240, true, 144) == 144);
static_assert(pose_sample_limit(false, 240, true, 165) == 165);
static_assert(pose_sample_limit(false, 240, false, 165) == 240);
} // namespace ui
