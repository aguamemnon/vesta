#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <thread>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <game/state/entities.hpp>
#include <game/state/runtime.hpp>
#include <game/state/pose_freshness.hpp>
#include <game/state/pose_schedule.hpp>
#include <foundation/snapshot_pool.hpp>
#include <cstddef>

namespace game
{

struct sampled_player_pose
{
    std::size_t source_index{};
    std::uintptr_t pawn{};
    std::uintptr_t bone_cache{};
    std::string_view model_path{};
    skeleton_reader::data bones{};
    // Timestamp of the successful bone read, retained across fallback reuse.
    std::chrono::steady_clock::time_point bone_captured_at{};
    bool reused_after_read_failure{};
};

struct player_pose_frame
{
    std::uint64_t sequence{};
    detail::pose_identity identity{};
    std::shared_ptr<const std::string> level{};
    // Acquisition begins here, but timestamp marks the immutable frame being published.
    std::chrono::steady_clock::time_point capture_started_at{};
    std::chrono::steady_clock::time_point timestamp{};
    std::shared_ptr<const std::vector<player_snapshot>> world{};
    presentation_camera_sample camera{};
    std::chrono::steady_clock::time_point camera_captured_at{};
    std::vector<sampled_player_pose> players{};
};

class player_pose_sampler
{
public:
    ~player_pose_sampler();
    player_pose_sampler() = default;
    player_pose_sampler(const player_pose_sampler&) = delete;
    player_pose_sampler& operator=(const player_pose_sampler&) = delete;
    void set_presentation_state(bool active, std::uint32_t sample_rate);
    [[nodiscard]] std::shared_ptr<const player_pose_frame> acquire_for_presentation();
    [[nodiscard]] std::shared_ptr<const player_pose_frame> latest() const;

private:
    struct cached_pose
    {
        std::uintptr_t bone_cache{};
        std::string model_path{};
        skeleton_reader::data bones{};
        std::chrono::steady_clock::time_point timestamp{};
    };

    void sample_once();
    void worker_loop(std::stop_token stop);
    void reset_worker_state();

    std::atomic<std::uint64_t> m_presentation{};
    detail::pose_identity m_last_identity{};
    std::shared_ptr<const std::string> m_last_level{};
    foundation::snapshot_pool<player_pose_frame> m_frames{};
    // Only the worker samples or touches the pose cache / snapshot pool.
    // Render thread sends rate/activity updates and only reads m_latest.
    std::atomic<std::uint32_t> m_rate{165};
    std::jthread m_worker{};
    std::mutex m_wake_mutex{};
    std::condition_variable m_wake{};
    detail::pose_sample_schedule m_schedule{};
    std::atomic<std::uint64_t> m_sequence{};
    std::atomic<std::shared_ptr<const player_pose_frame>> m_latest{};
    std::unordered_map<std::uintptr_t, cached_pose> m_last_valid{};
};

} // namespace game
