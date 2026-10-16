#include <stdafx.hpp>
#include <features/visuals/visuals.hpp>
#include <simulation/collision.hpp>
#include <render/presentation.hpp>
#include <game/state/services.hpp>
#include <game/state/constants.hpp>

namespace features::visuals
{
namespace
{
[[nodiscard]] bool finite3(const foundation::vec3& p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
[[nodiscard]] bool sensible_screen_point(const foundation::vec2& p)
{
    // Prevent near-plane projection explosions from being submitted to ImGui.
    return game::camera().projection_valid(p) && std::isfinite(p.x) &&
           std::isfinite(p.y) && std::fabs(p.x) < 20000.0f &&
           std::fabs(p.y) < 20000.0f;
}
} // namespace

void player_t::paint_threat_vision_zone(render::draw::draw_list& draw_list,
                                        const game::player_snapshot& player,
                                        const config::visual_profile::player::threat_vision_zone& cfg,
                                        const vesta_aa::Alert status, const double timestamp)
{
    if (!cfg.enabled || !player.pawn || player.health <= 0 ||
        !finite3(player.origin) || !std::isfinite(player.eye_angles.y) ||
        !std::isfinite(timestamp))
        return;

    const auto suspicious = status == vesta_aa::Alert::Suspicious ||
                            status == vesta_aa::Alert::RepeatedAnomalies;
    if (cfg.suspects_only && !suspicious)
        return;
    // When map collision is missing, render an explicitly approximate sector
    // rather than silently leaving the feature invisible.
    const bool clipped = cfg.clip_to_walls && simulation::collision().valid();

    const auto distance = std::clamp(cfg.distance_units, 150.0f, 2000.0f);
    const auto half_angle = std::clamp(cfg.half_angle_deg, 5.0f, 85.0f);
    const auto rays = std::clamp(cfg.rays, 6, 24);
    const auto refresh = std::clamp(cfg.refresh_seconds, 0.08f, 0.8f);

    if (m_threat_vision_cache.size() > 48)
        m_threat_vision_cache.clear();

    auto& cached = m_threat_vision_cache[player.pawn];
    const auto elapsed = timestamp - cached.updated_at;
    const auto moved = (player.origin - cached.origin).length_sqr();
    const auto rotated = std::fabs(foundation::wrap_yaw(player.eye_angles.y - cached.yaw));
    const auto recompute = cached.edge_points.size() != static_cast<std::size_t>(rays + 1) ||
                           cached.eye_edge_points.size() != static_cast<std::size_t>(rays + 1) ||
                           cached.updated_at < 0.0 || elapsed < 0.0 ||
                           cached.rays != rays || cached.walls != clipped ||
                           cached.ducked != player.is_ducked ||
                           std::fabs(cached.distance - distance) > 0.01f ||
                           std::fabs(cached.half_angle - half_angle) > 0.01f ||
                           elapsed >= refresh ||
                           (elapsed >= 0.09 && (moved > 32.0f * 32.0f || rotated > 8.0f));
    if (recompute)
    {
        cached.origin = player.origin;
        cached.yaw = player.eye_angles.y;
        cached.distance = distance;
        cached.half_angle = half_angle;
        cached.rays = rays;
        cached.walls = clipped;
        cached.ducked = player.is_ducked;
        cached.updated_at = timestamp;
        cached.edge_points.clear();
        cached.eye_edge_points.clear();
        cached.edge_points.reserve(static_cast<std::size_t>(rays + 1));
        cached.eye_edge_points.reserve(static_cast<std::size_t>(rays + 1));

        // We trace from approximate eye height against world geometry, then
        // draw a footprint at the pawn's foot-plane. Uneven terrain and
        // elevation changes mean this is NOT an exact line-of-sight volume.
        const auto eye = player.origin + foundation::vec3{0.0f, 0.0f,
                                                           player.is_ducked ? 46.0f : 64.0f};
        const auto floor_z = player.origin.z + 3.0f;
        for (int index = 0; index <= rays; ++index)
        {
            const auto angle = player.eye_angles.y - half_angle +
                               2.0f * half_angle * static_cast<float>(index) /
                                   static_cast<float>(rays);
            foundation::vec3 forward{}, right{}, up{};
            foundation::basis_from_angles(foundation::vec3{0.0f, angle, 0.0f}, forward, right, up);
            const auto far_point = eye + forward * distance;
            const auto end = clipped ? simulation::collision().trace_ray(eye, far_point).end_pos
                                     : far_point;
            cached.edge_points.emplace_back(end.x, end.y, floor_z);
            cached.eye_edge_points.push_back(end);
        }
    }

    const auto center = game::camera().project(cached.origin + foundation::vec3{0.0f, 0.0f, 3.0f});
    const auto eye_height = player.is_ducked ? 46.0f : 64.0f;
    const auto eye_screen = game::camera().project(
        cached.origin + foundation::vec3{0.0f, 0.0f, eye_height});
    if (!sensible_screen_point(center) && !sensible_screen_point(eye_screen))
        return;

    std::vector<foundation::vec2> projected;
    projected.reserve(cached.edge_points.size());
    for (const auto& world : cached.edge_points)
        projected.push_back(game::camera().project(world));

    const auto color = status == vesta_aa::Alert::RepeatedAnomalies
                           ? cfg.repeated_color
                           : status == vesta_aa::Alert::Suspicious ? cfg.suspect_color
                                                                    : cfg.normal_color;
    const auto angle_uncertain = player.eye_angles_fallback || !clipped;
    const auto opacity = std::clamp(player.legit_opacity, 0.0f, 1.0f) *
                         (angle_uncertain ? 0.72f : 1.0f);
    const auto fill_alpha = static_cast<std::uint8_t>(
        std::clamp(static_cast<float>(color.a) * 0.18f * opacity, 0.0f, 255.0f));
    const render::draw::rgba fill{color.r, color.g, color.b, fill_alpha};

    for (std::size_t i = 1; i < projected.size(); ++i)
    {
        if (!sensible_screen_point(center) ||
            !sensible_screen_point(projected[i-1]) || !sensible_screen_point(projected[i]))
            continue;

        if (cfg.filled)
        {
            // Render individual triangles: a polygon clipped by nearby walls
            // may be non-convex and cannot be passed to AddConvexPolyFilled.
            const std::array<float, 6> triangle{
                center.x, center.y,
                projected[i-1].x, projected[i-1].y,
                projected[i].x, projected[i].y
            };
            draw_list.add_convex_poly_filled(triangle, fill);
        }
        draw_list.add_line(projected[i-1].x, projected[i-1].y,
                           projected[i].x, projected[i].y, color, 1.7f);
    }

    if (sensible_screen_point(center) && !projected.empty() && sensible_screen_point(projected.front()))
        draw_list.add_line(center.x, center.y, projected.front().x,
                           projected.front().y, color, 1.0f);
    if (sensible_screen_point(center) && !projected.empty() && sensible_screen_point(projected.back()))
        draw_list.add_line(center.x, center.y, projected.back().x,
                           projected.back().y, color, 1.0f);

    // Elevated rays provide a usable marker even when the footprint is outside
    // the projection frustum (common when looking upwards at a close target).
    if (sensible_screen_point(eye_screen))
    {
        for (const std::size_t idx : {std::size_t{0}, cached.eye_edge_points.size() / 2,
                                      cached.eye_edge_points.size() - 1})
        {
            const auto endpoint = game::camera().project(cached.eye_edge_points[idx]);
            if (sensible_screen_point(endpoint))
                draw_list.add_line(eye_screen.x, eye_screen.y, endpoint.x, endpoint.y,
                                   color, idx == cached.eye_edge_points.size() / 2 ? 2.0f : 1.2f);
        }
        draw_list.add_circle(eye_screen.x, eye_screen.y, 5.0f, color, 12, 1.5f);
        if (const auto* font = render::fonts().esp_text_11)
        {
            const auto caption = angle_uncertain ? "VISION ~ (estimate)" : "VISION";
            draw_list.add_text(eye_screen.x + 8.0f, eye_screen.y - 18.0f, caption,
                               font, color, render::draw::text_style::outlined);
        }
    }
}
} // namespace features::visuals
