#include <stdafx.hpp>
#include <features/visuals/visuals.hpp>
#include <game/state/services.hpp>
#include <game/state/constants.hpp>

namespace features::visuals
{
namespace
{
[[nodiscard]] bool finite3(const foundation::vec3& p) noexcept
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
[[nodiscard]] bool onscreen(const foundation::vec2& p) noexcept
{
    if (!game::camera().projection_valid(p) ||
        !std::isfinite(p.x) || !std::isfinite(p.y))
        return false;
    const auto [width, height] = render::draw::get_display_size();
    constexpr float margin = 30.0f;
    return p.x >= -margin && p.y >= -margin &&
           p.x <= static_cast<float>(width) + margin &&
           p.y <= static_cast<float>(height) + margin;
}
} // namespace

void player_t::paint_threat_vision_zone(render::draw::draw_list& draw_list,
                                        const game::player_snapshot& player,
                                        const config::visual_profile::player::threat_vision_zone& cfg,
                                        const vesta_aa::Alert status, double /*timestamp*/)
{
    if (!cfg.enabled || !player.pawn || player.health <= 0 ||
        !finite3(player.origin) || !std::isfinite(player.eye_angles.y))
        return;

    const bool suspicious = status == vesta_aa::Alert::Suspicious ||
                            status == vesta_aa::Alert::RepeatedAnomalies;
    if (cfg.suspects_only && !suspicious)
        return;

    // Minimal mode only. The old clipping/filled wedge traced 6-24 rays per
    // player on the render thread and caused input latency. This visual
    // does not query collision geometry or allocate per-frame scratch arrays.
    // The displayed footprint is an approximate direction, not line-of-sight.
    const auto half_angle = std::clamp(cfg.half_angle_deg, 5.0f, 25.0f);
    const auto length = std::clamp(cfg.distance_units, 40.0f, 160.0f);
    const auto eye = player.origin + foundation::vec3{
        0.0f, 0.0f, player.is_ducked ? 46.0f : 64.0f};
    const auto from = game::camera().project(eye);
    if (!onscreen(from))
        return;

    const auto source = status == vesta_aa::Alert::RepeatedAnomalies
                            ? cfg.repeated_color
                            : status == vesta_aa::Alert::Suspicious
                                  ? cfg.suspect_color : cfg.normal_color;
    const render::draw::rgba outline{
        source.r, source.g, source.b,
        static_cast<std::uint8_t>(std::min(105, static_cast<int>(source.a)))};

    // Three tiny projected orientation lines instead of a large floor polygon.
    // At most three projections + three ImGui line calls for the sole nearest
    // target. Deliberately never trace rays against map geometry here.
    for (const float offset : {-half_angle, 0.0f, half_angle})
    {
        foundation::vec3 forward{}, right{}, up{};
        foundation::basis_from_angles(
            foundation::vec3{0.0f, player.eye_angles.y + offset, 0.0f},
            forward, right, up);
        const auto point = game::camera().project(
            eye + forward * (offset == 0.0f ? length : length * 0.7f));
        if (!onscreen(point))
            continue;
        draw_list.add_line(from.x, from.y, point.x, point.y, outline,
                           offset == 0.0f ? 1.5f : 1.0f);
    }
}
} // namespace features::visuals
