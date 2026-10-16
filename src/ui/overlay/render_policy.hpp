#pragma once
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
