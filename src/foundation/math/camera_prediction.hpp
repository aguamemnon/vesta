#pragma once

// Short, bounded visual-only camera lookahead. The result is a projected
// matrix for overlay drawing; it MUST NOT be published as gameplay camera
// state or used by input/aim decisions.
#include <foundation/math/vector.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>

namespace foundation
{
struct camera_prediction_sample
{
    matrix4 matrix{};
    vec3 origin{};
    vec3 angles{};
    float fov{};
};

class camera_projection_predictor
{
public:
    using clock = std::chrono::steady_clock;

    void reset() noexcept
    {
        m_previous = {};
        m_previous_at = {};
        m_previous_motion = {};
        m_had_previous_motion = false;
    }

    [[nodiscard]] std::optional<matrix4> observe(const camera_prediction_sample& current,
                                                 clock::time_point now, float desired_lead_ms) noexcept
    {
        std::optional<matrix4> projected{};
        const auto dt = std::chrono::duration<float, std::milli>(now - m_previous_at).count();
        if (m_previous_at != clock::time_point{} && dt >= 1.5f && dt <= 40.0f &&
            std::isfinite(current.fov) && std::isfinite(m_previous.fov) &&
            std::abs(current.fov - m_previous.fov) < 0.03f)
        {
            const auto wrapped = [](float newer, float older) noexcept
            { return std::remainder(newer - older, 360.0f); };
            const vec3 angle_delta{
                wrapped(current.angles.x, m_previous.angles.x),
                wrapped(current.angles.y, m_previous.angles.y),
                wrapped(current.angles.z, m_previous.angles.z)};
            const vec3 translation = current.origin - m_previous.origin;
            const auto rotation_deg = std::sqrt(angle_delta.length_sqr());
            const auto movement_units = std::sqrt(translation.length_sqr());
            // Suppress jumps/teleports, FOV changes and camera reversals.
            const auto reversed = m_had_previous_motion &&
                angle_delta.dot(m_previous_motion) < -0.025f;
            if (!reversed && rotation_deg < 14.0f && movement_units < 48.0f &&
                (rotation_deg > 0.025f || movement_units > 0.04f))
            {
                // Less than half the last sampling interval; at most 4 ms,
                // 1.5 degrees of view rotation or 3 game units of translation.
                auto lead = std::clamp(desired_lead_ms, 0.0f, 4.0f);
                lead = std::min(lead, dt * 0.5f);
                if (rotation_deg > 0.001f)
                    lead = std::min(lead, 1.5f * dt / rotation_deg);
                if (movement_units > 0.001f)
                    lead = std::min(lead, 3.0f * dt / movement_units);
                if (lead >= 0.1f)
                {
                    matrix4 result = current.matrix;
                    const auto ratio = lead / dt;
                    bool valid = true;
                    for (int r = 0; r < 4; ++r)
                        for (int c = 0; c < 4; ++c)
                        {
                            const auto a = current.matrix[r][c];
                            const auto b = m_previous.matrix[r][c];
                            result[r][c] = a + (a - b) * ratio;
                            valid &= std::isfinite(a) && std::isfinite(b) &&
                                     std::isfinite(result[r][c]);
                        }
                    if (valid)
                        projected = result;
                }
            }
            m_previous_motion = angle_delta;
            m_had_previous_motion = true;
        }
        else
        {
            m_previous_motion = {};
            m_had_previous_motion = false;
        }
        m_previous = current;
        m_previous_at = now;
        return projected;
    }

private:
    camera_prediction_sample m_previous{};
    clock::time_point m_previous_at{};
    vec3 m_previous_motion{};
    bool m_had_previous_motion{};
};
} // namespace foundation
