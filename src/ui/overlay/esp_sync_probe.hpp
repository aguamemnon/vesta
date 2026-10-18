#pragma once

// Opt-in, passive ESP/camera synchronization diagnostic. No game input is
// changed, and no file is written until the user clicks Export in the menu.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <system_error>

#include <diagnostics/log_directory.hpp>
#include <game/state/pose.hpp>
#include <game/state/services.hpp>
#include <imgui.h>

namespace ui::esp_sync
{
// Timing breakdown measured around Present(0, DO_NOT_WAIT) only while
// the probe is enabled. No GPU timestamp query and no additional wait.
struct present_metrics
{
    float total_ms{};
    float native_dxgi_ms{};
    float retry_wait_ms{};
    std::uint32_t calls{};
    std::uint32_t still_drawing{};
};

class probe
{
    using clock = std::chrono::steady_clock;

    struct row
    {
        float elapsed_ms{};
        float pose_age_ms{};
        float world_age_ms{};
        float bone_age_ms{};
        float camera_age_ms{};
        float camera_gap_ms{};
        float camera_shift_px{};
        float present_call_ms{};
        int simulation_tick{};
        bool fallback_bones{};
        bool present_dropped{};
        std::uint64_t pose_sequence{};
        // Added in v1.2.10; previous columns remain in their original order.
        float frame_build_ms{};
        float pose_to_present_ms{};
        float world_to_present_ms{};
        float bone_to_present_ms{};
        float native_dxgi_present_ms{};
        float present_retry_wait_ms{};
        std::uint32_t present_calls{};
        std::uint32_t present_retries{};
        std::uint32_t draw_frames{};
        std::uint32_t repeated_pose_frames{};
        float simulation_time{};
        float sim_unchanged_ms{-1.0f};
        bool simulation_valid{};
        float pacing_wait_ms{};
        bool performance_mode{};
        bool gpu_effects{};
        std::uint32_t display_refresh_hz{};
        std::uint32_t pose_target_hz{};
        std::uint32_t gpu_vendor_id{};
    };

    struct capture
    {
        bool valid{};
        clock::time_point pose_at{};
        clock::time_point world_at{};
        clock::time_point bone_at{};
        clock::time_point camera_at{};
        std::uint64_t sequence{};
        int simulation_tick{-1};
        float simulation_time{};
        bool simulation_valid{};
        bool fallback_bones{};
        float camera_gap_ms{-1.0f};
        float camera_shift_px{};
    };

    static constexpr std::size_t k_capacity = 1800; // 3 minutes @ 10 samples/s
    std::array<row, k_capacity> m_rows{};
    std::size_t m_head{};
    std::size_t m_count{};
    clock::time_point m_started{};
    clock::time_point m_last_record{};
    float m_present_ms{};
    bool m_present_dropped{};
    std::uint64_t m_present_drops{};
    present_metrics m_last_present{};
    float m_last_build_ms{};
    float m_last_pacing_wait_ms{};
    float m_last_pose_to_present_ms{-1.0f};
    std::uint64_t m_last_pose_sequence{};
    bool m_last_pose_valid{};
    std::uint32_t m_draw_frames_since_record{};
    std::uint32_t m_reused_pose_frames_since_record{};
    capture m_pending{};
    // A simulation tick of -1 is invalid in current captures. Track changes
    // in simulation time when present, otherwise mark the metric unavailable.
    std::uintptr_t m_last_sim_pawn{};
    int m_last_sim_tick{-1};
    float m_last_sim_time{};
    clock::time_point m_last_sim_change{};
    float m_last_sim_unchanged_ms{-1.0f};

    [[nodiscard]] static float age_ms(clock::time_point now, clock::time_point observed) noexcept
    {
        if (observed == clock::time_point{} || now < observed)
            return -1.0f;
        return std::chrono::duration<float, std::milli>(now - observed).count();
    }

    // Exactly the same projection convention and near-plane policy as
    // game::camera_state::project; this does NOT modify the render camera.
    [[nodiscard]] static std::optional<ImVec2> project(
        const foundation::matrix4& m, const foundation::vec3& p, float width, float height) noexcept
    {
        if (m[3][3] == 0.0f)
            return std::nullopt;
        const float w = m[3][0] * p.x + m[3][1] * p.y + m[3][2] * p.z + m[3][3];
        if (!std::isfinite(w) || w < 7.0f)
            return std::nullopt;
        const float x = (m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3]) / w;
        const float y = (m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3]) / w;
        if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 64.0f || std::abs(y) > 64.0f)
            return std::nullopt;
        return ImVec2(width * 0.5f * (1.0f + x), height * 0.5f * (1.0f - y));
    }

public:
    void clear() noexcept
    {
        m_head = m_count = 0;
        m_started = m_last_record = {};
        m_present_ms = 0.0f;
        m_present_dropped = false;
        m_present_drops = 0;
        m_last_present = {};
        m_last_build_ms = 0.0f;
        m_last_pacing_wait_ms = 0.0f;
        m_last_pose_to_present_ms = -1.0f;
        m_last_pose_sequence = 0;
        m_last_pose_valid = false;
        m_draw_frames_since_record = m_reused_pose_frames_since_record = 0;
        m_pending = {};
        m_last_sim_pawn = 0;
        m_last_sim_tick = -1;
        m_last_sim_time = 0.0f;
        m_last_sim_change = {};
        m_last_sim_unchanged_ms = -1.0f;
    }

    [[nodiscard]] std::size_t count() const noexcept { return m_count; }

    void on_present(const present_metrics& metrics, float frame_build_ms, float pacing_wait_ms,
                    bool dropped, bool performance_mode, bool gpu_effects,
                    std::uint32_t display_refresh_hz, std::uint32_t pose_target_hz,
                    std::uint32_t gpu_vendor_id) noexcept
    {
        const auto now = clock::now();
        m_last_present = metrics;
        m_last_build_ms = std::max(0.0f, frame_build_ms);
        m_last_pacing_wait_ms = std::max(0.0f, pacing_wait_ms);
        m_present_ms = metrics.total_ms;
        m_present_dropped = dropped;
        if (dropped)
            ++m_present_drops;
        m_last_pose_to_present_ms = m_pending.valid ? age_ms(now, m_pending.pose_at) : -1.0f;

        if (!m_pending.valid)
            return;
        if (m_started == clock::time_point{})
            m_started = now;
        if (m_last_record != clock::time_point{} && now - m_last_record < std::chrono::milliseconds(100))
            return;

        m_last_record = now;
        row r{};
        r.elapsed_ms = age_ms(now, m_started);
        r.pose_age_ms = age_ms(now, m_pending.pose_at);
        r.world_age_ms = age_ms(now, m_pending.world_at);
        r.bone_age_ms = age_ms(now, m_pending.bone_at);
        r.camera_age_ms = age_ms(now, m_pending.camera_at);
        r.camera_gap_ms = m_pending.camera_gap_ms;
        r.camera_shift_px = m_pending.camera_shift_px;
        r.present_call_ms = metrics.total_ms;
        r.simulation_tick = m_pending.simulation_tick;
        r.fallback_bones = m_pending.fallback_bones;
        r.present_dropped = dropped;
        r.pose_sequence = m_pending.sequence;
        r.frame_build_ms = m_last_build_ms;
        r.pose_to_present_ms = age_ms(now, m_pending.pose_at);
        r.world_to_present_ms = age_ms(now, m_pending.world_at);
        r.bone_to_present_ms = age_ms(now, m_pending.bone_at);
        r.native_dxgi_present_ms = metrics.native_dxgi_ms;
        r.present_retry_wait_ms = metrics.retry_wait_ms;
        r.present_calls = metrics.calls;
        r.present_retries = metrics.still_drawing;
        r.draw_frames = m_draw_frames_since_record;
        r.repeated_pose_frames = m_reused_pose_frames_since_record;
        r.simulation_time = m_pending.simulation_time;
        r.simulation_valid = m_pending.simulation_valid;
        r.sim_unchanged_ms = m_last_sim_unchanged_ms;
        r.pacing_wait_ms = m_last_pacing_wait_ms;
        r.performance_mode = performance_mode;
        r.gpu_effects = gpu_effects;
        r.display_refresh_hz = display_refresh_hz;
        r.pose_target_hz = pose_target_hz;
        r.gpu_vendor_id = gpu_vendor_id;
        m_rows[m_head] = r;
        m_head = (m_head + 1) % k_capacity;
        m_count = std::min(m_count + 1, k_capacity);
        m_draw_frames_since_record = m_reused_pose_frames_since_record = 0;
    }

    // Called only while the opt-in diagnostic toggle is enabled. At most one
    // enemy head is projected in each view; the exported 10 Hz ring is bounded.
    void draw(const std::shared_ptr<const game::player_pose_frame>& frame,
              const game::presentation_camera_sample* live_camera, std::uint32_t width, std::uint32_t height,
              clock::time_point live_camera_at)
    {
        if (!width || !height)
            return;
        const auto now = clock::now();
        m_pending = {};
        auto* canvas = ImGui::GetForegroundDrawList();
        const auto w = static_cast<float>(width);
        const auto h = static_cast<float>(height);
        const auto panel_x = std::max(8.0f, w - 350.0f);
        const ImVec2 top{panel_x, 22.0f};
        const ImVec2 bottom{panel_x + 338.0f, 179.0f};
        canvas->AddRectFilled(top, bottom, IM_COL32(9, 13, 20, 216), 8.0f);
        canvas->AddRect(top, bottom, IM_COL32(120, 140, 159, 100), 8.0f);

        const auto print = [canvas, panel_x](float y, ImU32 color, const char* value)
        { canvas->AddText(ImVec2{panel_x + 12.0f, y}, color, value); };

        print(30.0f, IM_COL32(242, 244, 249, 255), "ESP SYNC PROBE | passive 10 Hz");
        if (!frame || !frame->world || !live_camera)
        {
            print(53.0f, IM_COL32(242, 175, 98, 255),
                  !frame || !frame->world ? "Waiting for a valid pose frame" : "Current camera unavailable");
            print(77.0f, IM_COL32(176, 188, 205, 255), "Enable player ESP / enter a match");
            return;
        }

        // Rendering can reuse a previously acquired pose several times.
        // Count actual drawn frames, independently of the 10 Hz CSV cadence.
        ++m_draw_frames_since_record;
        if (m_last_pose_valid && frame->sequence == m_last_pose_sequence)
            ++m_reused_pose_frames_since_record;
        m_last_pose_sequence = frame->sequence;
        m_last_pose_valid = true;

        const game::sampled_player_pose* selected{};
        const game::player_snapshot* selected_player{};
        ImVec2 live_position{}, pose_position{};
        auto best_distance = std::numeric_limits<float>::max();

        for (const auto& candidate : frame->players)
        {
            if (candidate.source_index >= frame->world->size() || !candidate.bones.is_valid())
                continue;
            const auto& player = (*frame->world)[candidate.source_index];
            if (!game::local_player().is_enemy(player.team))
                continue;
            const auto head = candidate.bones.get_position(7);
            if (!std::isfinite(head.x) || !std::isfinite(head.y) || !std::isfinite(head.z))
                continue;
            const auto current = project(live_camera->matrix, head, w, h);
            const auto captured = project(frame->camera.matrix, head, w, h);
            if (!current || !captured || current->x < -25.0f || current->y < -25.0f ||
                current->x > w + 25.0f || current->y > h + 25.0f)
                continue;
            const auto dx = current->x - w * 0.5f;
            const auto dy = current->y - h * 0.5f;
            const auto distance = dx * dx + dy * dy;
            if (distance >= best_distance)
                continue;
            best_distance = distance;
            selected = &candidate;
            selected_player = &player;
            live_position = *current;
            pose_position = *captured;
        }

        if (!selected)
        {
            print(53.0f, IM_COL32(242, 175, 98, 255), "No visible enemy pose for comparison");
            print(77.0f, IM_COL32(176, 188, 205, 255), "Aim near a bot and strafe left/right");
            return;
        }

        const float shift = std::hypot(live_position.x - pose_position.x, live_position.y - pose_position.y);
        const float pose_age = age_ms(now, frame->timestamp);
        const float world_age = age_ms(now, selected_player->sampled_at);
        const float bone_age = age_ms(now, selected->bone_captured_at);
        const float camera_age = age_ms(now, frame->camera_captured_at);
        const float gap = age_ms(live_camera_at, frame->camera_captured_at);

        canvas->AddLine(live_position, pose_position, IM_COL32(242, 183, 102, 175), 1.5f);
        canvas->AddCircle(live_position, 6.0f, IM_COL32(78, 215, 236, 245), 20, 2.0f);
        canvas->AddLine({pose_position.x - 5.0f, pose_position.y - 5.0f},
                        {pose_position.x + 5.0f, pose_position.y + 5.0f}, IM_COL32(255, 175, 85, 255), 2.0f);
        canvas->AddLine({pose_position.x - 5.0f, pose_position.y + 5.0f},
                        {pose_position.x + 5.0f, pose_position.y - 5.0f}, IM_COL32(255, 175, 85, 255), 2.0f);

        char line[160]{};
        std::snprintf(line, sizeof(line), "Pose %.1fms | World %.1fms | Bones %.1fms",
                      pose_age, world_age, bone_age);
        print(51.0f, IM_COL32(210, 219, 229, 255), line);
        std::snprintf(line, sizeof(line), "Camera %.1fms | Cam gap %.1fms | Delta %.1fpx",
                      camera_age, gap, shift);
        print(71.0f, IM_COL32(210, 219, 229, 255), line);
        std::snprintf(line, sizeof(line), "Fallback %s | Present call %.2fms | Drops %llu",
                      selected->reused_after_read_failure ? "YES" : "no", m_present_ms,
                      static_cast<unsigned long long>(m_present_drops));
        print(91.0f, selected->reused_after_read_failure ? IM_COL32(249, 172, 90, 255)
                                                        : IM_COL32(210, 219, 229, 255), line);
        std::snprintf(line, sizeof(line), "DXGI %.1fms | Retry wait %.1fms | Reuse %u/%u",
                      m_last_present.native_dxgi_ms, m_last_present.retry_wait_ms,
                      m_reused_pose_frames_since_record, m_draw_frames_since_record);
        print(111.0f, IM_COL32(210, 219, 229, 255), line);
        std::snprintf(line, sizeof(line), "Pose->Present %.1fms | Build %.1fms | Pace %.1fms",
                      m_last_pose_to_present_ms, m_last_build_ms, m_last_pacing_wait_ms);
        print(130.0f, IM_COL32(210, 219, 229, 255), line);
        print(151.0f, IM_COL32(78, 215, 236, 245), "O live camera");
        print(165.0f, IM_COL32(255, 175, 85, 255), "X pose camera, same bone");

        // The target snapshot is staged for on_present(), so all measurements
        // in each CSV row are from a single render-to-present attempt.
        const auto valid_sim_tick = selected_player->simulation_tick >= 0;
        const auto valid_sim_time = std::isfinite(selected_player->simulation_time) &&
                                    selected_player->simulation_time > 0.0f;
        const bool sim_valid = valid_sim_tick || valid_sim_time;
        if (selected_player->pawn != m_last_sim_pawn || !sim_valid)
        {
            m_last_sim_pawn = selected_player->pawn;
            m_last_sim_tick = selected_player->simulation_tick;
            m_last_sim_time = selected_player->simulation_time;
            m_last_sim_change = sim_valid ? now : clock::time_point{};
        }
        else if ((valid_sim_tick && m_last_sim_tick != selected_player->simulation_tick) ||
                 (valid_sim_time && std::abs(m_last_sim_time - selected_player->simulation_time) > 0.00001f))
        {
            m_last_sim_tick = selected_player->simulation_tick;
            m_last_sim_time = selected_player->simulation_time;
            m_last_sim_change = now;
        }
        m_last_sim_unchanged_ms = sim_valid ? age_ms(now, m_last_sim_change) : -1.0f;
        m_pending = {
            .valid = true,
            .pose_at = frame->timestamp,
            .world_at = selected_player->sampled_at,
            .bone_at = selected->bone_captured_at,
            .camera_at = frame->camera_captured_at,
            .sequence = frame->sequence,
            .simulation_tick = selected_player->simulation_tick,
            .simulation_time = valid_sim_time ? selected_player->simulation_time : 0.0f,
            .simulation_valid = sim_valid,
            .fallback_bones = selected->reused_after_read_failure,
            .camera_gap_ms = gap,
            .camera_shift_px = shift
        };
    }

    // Only called by the explicit Save button in Misc. No automatic telemetry
    // or network transmission. No player names, IDs or coordinates in CSV.
    [[nodiscard]] bool save_csv() const
    {
        if (!m_count)
            return false;
        const auto directory = diagnostics::default_logs_directory();
        if (directory.empty())
            return false;
        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        if (ec)
            return false;
        std::ofstream output(directory / L"vesta-esp-sync.csv", std::ios::out | std::ios::trunc);
        if (!output)
            return false;
        output << "elapsed_ms,pose_age_ms,world_age_ms,bone_age_ms,pose_camera_age_ms,"
                  "camera_gap_ms,camera_projection_shift_px,present_call_ms,"
                  "simulation_tick,fallback_bones,present_dropped,pose_sequence,"
                  "frame_build_ms,pose_to_present_ms,world_to_present_ms,bone_to_present_ms,"
                  "native_dxgi_present_ms,present_retry_wait_ms,present_calls,present_retries,"
                  "draw_frames,repeated_pose_frames,simulation_time,simulation_valid,sim_unchanged_ms,"
                  "pacing_wait_ms,performance_mode,gpu_effects,display_refresh_hz,"
                  "pose_target_hz,gpu_vendor_id\n";
        const auto start = (m_head + k_capacity - m_count) % k_capacity;
        for (std::size_t i = 0; i < m_count; ++i)
        {
            const auto& s = m_rows[(start + i) % k_capacity];
            output << s.elapsed_ms << ',' << s.pose_age_ms << ',' << s.world_age_ms << ','
                   << s.bone_age_ms << ',' << s.camera_age_ms << ',' << s.camera_gap_ms << ','
                   << s.camera_shift_px << ',' << s.present_call_ms << ','
                   << s.simulation_tick << ',' << (s.fallback_bones ? 1 : 0) << ','
                   << (s.present_dropped ? 1 : 0) << ',' << s.pose_sequence << ','
                   << s.frame_build_ms << ',' << s.pose_to_present_ms << ','
                   << s.world_to_present_ms << ',' << s.bone_to_present_ms << ','
                   << s.native_dxgi_present_ms << ',' << s.present_retry_wait_ms << ','
                   << s.present_calls << ',' << s.present_retries << ','
                   << s.draw_frames << ',' << s.repeated_pose_frames << ','
                   << s.simulation_time << ',' << (s.simulation_valid ? 1 : 0) << ','
                   << s.sim_unchanged_ms << ',' << s.pacing_wait_ms << ','
                   << (s.performance_mode ? 1 : 0) << ',' << (s.gpu_effects ? 1 : 0) << ','
                   << s.display_refresh_hz << ','
                   << s.pose_target_hz << ',' << s.gpu_vendor_id << '\n';
        }
        output.flush();
        return output.good();
    }
};

[[nodiscard]] inline probe& bound_probe() noexcept
{
    static probe instance{};
    return instance;
}

} // namespace ui::esp_sync
