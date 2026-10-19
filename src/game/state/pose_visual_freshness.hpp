#pragma once

// Presentation-only freshness policy. Keep this independent from the game
// sampler's 64 Hz cadence, combat paths and DX11 frame pacing.
#include <chrono>

namespace game::detail
{
using visual_clock = std::chrono::steady_clock;

// Just over three 64 Hz world cycles. A missed sampler iteration may be
// bridged briefly, but it cannot be mistaken for fresh spatial information.
inline constexpr auto visual_world_lifetime = std::chrono::milliseconds(50);
// A cached bone set may outlive one failed read; do not render it for the
// full 100 ms lifetime of the existing general cache.
inline constexpr auto visual_fallback_bone_lifetime = std::chrono::milliseconds(50);

[[nodiscard]] inline bool visual_timestamp_fresh(
    visual_clock::time_point observed, visual_clock::time_point now,
    visual_clock::duration lifetime) noexcept
{
    // Steady-clock epochs, default timestamps and future samples are invalid.
    return observed != visual_clock::time_point{} && now >= observed &&
           now - observed <= lifetime;
}

[[nodiscard]] inline bool visual_world_fresh(
    visual_clock::time_point sampled, visual_clock::time_point now) noexcept
{
    return visual_timestamp_fresh(sampled, now, visual_world_lifetime);
}

[[nodiscard]] inline bool visual_fallback_bone_fresh(
    visual_clock::time_point sampled, visual_clock::time_point now) noexcept
{
    return visual_timestamp_fresh(sampled, now, visual_fallback_bone_lifetime);
}
} // namespace game::detail
