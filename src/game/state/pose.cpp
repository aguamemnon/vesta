#include <stdafx.hpp>
#include <diagnostics/performance.hpp>
#include <game/state/services.hpp>

#include <game/state/pose.hpp>
#include <game/state/pose_visual_freshness.hpp>
#include <game/state/level.hpp>

namespace game
{

namespace
{
constexpr auto k_pose_fallback_lifetime = detail::pose_fallback_lifetime;

detail::pose_identity pose_context(const local_snapshot& local, const std::shared_ptr<const std::string>& level,
                                   std::uint64_t presentation)
{
    return {presentation,
            local.controller,
            local.pawn,
            local.alive ? local.pawn : local.observer_pawn,
            reinterpret_cast<std::uintptr_t>(level.get()),
            local.pawn_handle,
            local.view_team,
            local.team_mode};
}
} // namespace

player_pose_sampler::~player_pose_sampler()
{
    // Join before mutex/condition members are destroyed. At most one bounded
    // in-flight memory read can finish while shutdown is requested.
    if (m_worker.joinable())
    {
        m_worker.request_stop();
        m_wake.notify_one();
        m_worker.join();
    }
}

void player_pose_sampler::set_presentation_state(const bool active, const std::uint32_t sample_rate)
{
    // Called by render thread only. The producer exclusively owns its caches
    // and buffers; changing presentation state never mutates them here.
    const auto previous_rate = m_rate.exchange(sample_rate, std::memory_order_acq_rel);
    const auto state = m_presentation.load(std::memory_order_acquire);
    const bool changed = static_cast<bool>(state & 1u) != active;
    if (changed)
    {
        const auto next = ((state + 2u) & ~std::uint64_t{1}) | std::uint64_t{active};
        m_presentation.store(next, std::memory_order_release);
        m_latest.store({}, std::memory_order_release);
    }

    if (active && !m_worker.joinable())
        m_worker = std::jthread([this](std::stop_token stop) { worker_loop(stop); });
    if (changed || previous_rate != sample_rate)
        m_wake.notify_one();
}

void player_pose_sampler::reset_worker_state()
{
    m_latest.store({}, std::memory_order_release);
    m_last_valid.clear();
    m_last_identity = {};
    m_last_level.reset();
    m_frames.clear();
    m_schedule.reset();
}

void player_pose_sampler::worker_loop(const std::stop_token stop)
{
    // Native wait is interruptible by activation, rate changes and shutdown.
    // Remote bone reads never execute on the render / DXGI thread.
    std::uint64_t observed_presentation = 0;
    while (!stop.stop_requested())
    {
        const auto presentation = m_presentation.load(std::memory_order_acquire);
        if (presentation != observed_presentation)
        {
            reset_worker_state();
            observed_presentation = presentation;
        }
        if (!(presentation & 1u))
        {
            std::unique_lock lock(m_wake_mutex);
            m_wake.wait(lock, [&] {
                return stop.stop_requested() ||
                       m_presentation.load(std::memory_order_acquire) != presentation;
            });
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto rate = m_rate.load(std::memory_order_acquire);
        m_schedule.configure(rate, now);
        if (m_schedule.due(now))
        {
            sample_once();
            m_schedule.sampled(std::chrono::steady_clock::now());
        }
        else
        {
            std::unique_lock lock(m_wake_mutex);
            m_wake.wait_until(lock, m_schedule.next_due(), [&] {
                return stop.stop_requested() ||
                       m_presentation.load(std::memory_order_acquire) != presentation ||
                       m_rate.load(std::memory_order_acquire) != rate;
            });
        }
    }
    m_latest.store({}, std::memory_order_release);
}

std::shared_ptr<const player_pose_frame> player_pose_sampler::acquire_for_presentation()
{
    // Latest immutable snapshot only. The render thread never waits on reads.
    return latest();
}

std::shared_ptr<const player_pose_frame> player_pose_sampler::latest() const
{
    const auto presentation = m_presentation.load(std::memory_order_acquire);
    if (!(presentation & 1u))
        return {};
    const auto frame = m_latest.load(std::memory_order_acquire);
    const auto local = game::local_player().snapshot();
    const auto level = game::level().name();
    if (!frame || !local ||
        !detail::pose_frame_usable(frame->identity, pose_context(*local, level, presentation), frame->timestamp,
                                   std::chrono::steady_clock::now()) ||
        m_presentation.load(std::memory_order_acquire) != presentation)
        return {};
    return frame;
}

void player_pose_sampler::sample_once()
{
    VESTA_PERF_SCOPE(pose_sample);
    const auto now = std::chrono::steady_clock::now();
    const auto presentation = m_presentation.load(std::memory_order_acquire);
    const auto local = game::local_player().snapshot();
    const auto level = game::level().name();
    const auto identity = local ? pose_context(*local, level, presentation) : detail::pose_identity{};
    if (identity != m_last_identity)
    {
        m_last_valid.clear();
        m_latest.store({}, std::memory_order_release);
        m_last_identity = identity;
        m_last_level = level;
    }
    if (!detail::valid_pose_identity(identity))
        return;
    auto frame = m_frames.acquire();
    frame->players.clear();
    frame->camera = {};
    frame->camera_captured_at = {};
    frame->identity = identity;
    frame->level = level;
    frame->sequence = m_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    frame->capture_started_at = now;
    frame->timestamp = now;
    frame->world = game::world().players();
    if (!frame->world)
    {
        return;
    }

    frame->players.reserve(frame->world->size());
    const auto local_pawn = identity.pawn;
    const auto view_pawn = identity.view_pawn;
    for (std::size_t index = 0; index < frame->world->size(); ++index)
    {
        const auto& player = (*frame->world)[index];
        if (!player.pawn || !player.bone_cache || player.health <= 0)
        {
            continue;
        }
        if (player.pawn == local_pawn || player.pawn == view_pawn ||
            (identity.team_mode && identity.view_team == player.team))
        {
            continue;
        }

        auto bones = game::skeletons().get(player.bone_cache);
        bool reused{};
        auto bone_captured_at = std::chrono::steady_clock::now();
        if (bones.is_valid())
        {
            m_last_valid[player.pawn] = {player.bone_cache, player.model_path, bones, bone_captured_at};
        }
        else
        {
            const auto cached = m_last_valid.find(player.pawn);
            if (cached == m_last_valid.end() || cached->second.bone_cache != player.bone_cache ||
                cached->second.model_path != player.model_path ||
                now - cached->second.timestamp > k_pose_fallback_lifetime)
            {
                continue;
            }
            bones = cached->second.bones;
            bone_captured_at = cached->second.timestamp;
            reused = true;
        }

        frame->players.push_back({index, player.pawn, player.bone_cache, player.model_path, bones,
                                  bone_captured_at, reused});
    }

    std::erase_if(m_last_valid,
                  [&](const auto& entry) { return now - entry.second.timestamp > k_pose_fallback_lifetime; });

    if (!game::camera().sample_presentation(frame->camera))
        return;
    frame->camera_captured_at = std::chrono::steady_clock::now();
    const auto final_local = game::local_player().snapshot();
    const auto final_level = game::level().name();
    const auto final_presentation = m_presentation.load(std::memory_order_acquire);
    if (!final_local ||
        !detail::pose_frame_usable(identity, pose_context(*final_local, final_level, final_presentation), now,
                                   std::chrono::steady_clock::now()))
        return;
    // Age the published immutable frame from the end of acquisition. Earlier
    // timestamps already describe the world snapshot and individual bone
    // reads; this avoids treating time spent in the worker as render latency.
    frame->timestamp = std::chrono::steady_clock::now();
    m_latest.store(std::move(frame), std::memory_order_release);
}

} // namespace game
