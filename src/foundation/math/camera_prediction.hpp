#pragma once

// Render-only rigid camera lookahead. Never publish the extrapolated matrix
// as game state, and never use it in input/aim calculations.
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
        // Reject corrupted samples before they can poison the next frame.
        if (!valid(current) || !std::isfinite(desired_lead_ms))
        {
            reset();
            return std::nullopt;
        }

        std::optional<matrix4> projected{};
        const auto dt = m_previous_at == clock::time_point{}
                            ? 0.0f
                            : std::chrono::duration<float, std::milli>(now - m_previous_at).count();
        if (m_previous_at != clock::time_point{} && dt >= 1.5f && dt <= 40.0f &&
            std::abs(current.fov - m_previous.fov) < 0.03f)
        {
            const auto wrapped = [](float newer, float older) noexcept
            { return std::remainder(newer - older, 360.0f); };
            const vec3 angles{
                wrapped(current.angles.x, m_previous.angles.x),
                wrapped(current.angles.y, m_previous.angles.y),
                wrapped(current.angles.z, m_previous.angles.z)};
            const vec3 travel = current.origin - m_previous.origin;
            const float rotation_deg = std::sqrt(angles.length_sqr());
            const float travel_units = std::sqrt(travel.length_sqr());
            const bool reversed = m_had_previous_motion && angles.dot(m_previous_motion) < -0.025f;
            if (!reversed && rotation_deg < 14.0f && travel_units < 48.0f &&
                (rotation_deg > 0.025f || travel_units > 0.04f))
            {
                float lead_ms = std::clamp(desired_lead_ms, 0.0f, 4.0f);
                lead_ms = std::min(lead_ms, dt * 0.5f);
                if (rotation_deg > 0.001f)
                    lead_ms = std::min(lead_ms, 1.5f * dt / rotation_deg);
                if (travel_units > 0.001f)
                    lead_ms = std::min(lead_ms, 3.0f * dt / travel_units);
                if (lead_ms >= 0.1f)
                    projected = predict(m_previous, current, lead_ms / dt, travel, rotation_deg);
            }
            m_previous_motion = angles;
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
    static bool finite(vec3 p) noexcept
    {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    }

    static bool valid(const camera_prediction_sample& v) noexcept
    {
        if (!finite(v.origin) || !finite(v.angles) || !std::isfinite(v.fov) ||
            v.fov <= 1.0f || v.fov >= 179.0f)
            return false;
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                if (!std::isfinite(v.matrix[r][c]))
                    return false;
        return true;
    }

    static vec3 axis(const matrix4& m, int row) noexcept
    {
        return {m[row][0], m[row][1], m[row][2]};
    }

    static bool basis(const matrix4& m, vec3& right, vec3& up, vec3& forward) noexcept
    {
        right = axis(m, 0);
        up = axis(m, 1);
        forward = axis(m, 3);
        const float rs = right.length_sqr();
        const float us = up.length_sqr();
        const float fs = forward.length_sqr();
        if (rs < 1.0e-8f || us < 1.0e-8f || fs < 1.0e-8f)
            return false;
        right = right / std::sqrt(rs);
        up = up / std::sqrt(us);
        forward = forward / std::sqrt(fs);
        // A pinhole camera has mutually perpendicular right/up/forward.
        // Bypass unusual/off-axis projections rather than corrupt them.
        return std::abs(right.dot(up)) < 0.12f &&
               std::abs(right.dot(forward)) < 0.12f &&
               std::abs(up.dot(forward)) < 0.12f;
    }

    static vec3 rotate(vec3 p, vec3 unit_axis, float sine, float cosine) noexcept
    {
        return p * cosine + unit_axis.cross(p) * sine +
               unit_axis * (unit_axis.dot(p) * (1.0f - cosine));
    }

    [[nodiscard]] static std::optional<matrix4> predict(
        const camera_prediction_sample& previous, const camera_prediction_sample& current,
        float fraction, vec3 travel, float rotation_deg) noexcept
    {
        vec3 old_right{}, old_up{}, old_forward{};
        vec3 new_right{}, new_up{}, new_forward{};
        if (!basis(previous.matrix, old_right, old_up, old_forward) ||
            !basis(current.matrix, new_right, new_up, new_forward))
            return std::nullopt;

        // For a small rigid camera turn, the mean cross of three orthonormal
        // axes estimates its world-space angular displacement. Apply a single
        // Rodrigues rotation to *every* row, preserving row lengths and the
        // depth/clip-plane relationship instead of interpolating 16 elements.
        const vec3 delta_axis =
            (old_right.cross(new_right) + old_up.cross(new_up) +
             old_forward.cross(new_forward)) * 0.5f;
        const float angle = std::sqrt(delta_axis.length_sqr());
        if (!std::isfinite(angle) || angle > 0.5f)
            return std::nullopt;
        // Some game snapshots can contain new view angles but an old view
        // matrix. Predicting across that mismatched pair is unsafe.
        if (rotation_deg > 0.3f &&
            std::abs(to_degrees(angle) - rotation_deg) > std::max(0.75f, rotation_deg * 0.65f))
            return std::nullopt;
        const vec3 unit_axis = angle > 1.0e-6f ? delta_axis / angle : vec3{};
        const float applied = angle * fraction;
        const float sine = std::sin(applied);
        const float cosine = std::cos(applied);
        const vec3 future_origin = current.origin + travel * fraction;

        matrix4 result = current.matrix;
        for (int row = 0; row < 4; ++row)
        {
            const vec3 previous_row = axis(current.matrix, row);
            const vec3 predicted_row = angle > 1.0e-6f
                                           ? rotate(previous_row, unit_axis, sine, cosine)
                                           : previous_row;
            if (!finite(predicted_row))
                return std::nullopt;
            result[row][0] = predicted_row.x;
            result[row][1] = predicted_row.y;
            result[row][2] = predicted_row.z;
            // Keep the intrinsic row offset constant when advancing the eye:
            // row.w + row.xyz dot camera_origin = camera-independent term.
            result[row][3] = current.matrix[row][3] +
                             previous_row.dot(current.origin) -
                             predicted_row.dot(future_origin);
            if (!std::isfinite(result[row][3]))
                return std::nullopt;
        }
        return result;
    }

    camera_prediction_sample m_previous{};
    clock::time_point m_previous_at{};
    vec3 m_previous_motion{};
    bool m_had_previous_motion{};
};
} // namespace foundation
