#pragma once

#include <features/visuals/anti_aim_observer.hpp>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vesta_aa
{
// Observations only. No attacker attribution or server-side hit confirmation.
enum class DuelEvent : std::uint8_t {
    EnemyFired, EnemyDamaged, EnemyDied, LocalDamaged, LocalDied, AlertChanged
};

struct DuelEventRecord {
    double recorded_at_sec{};
    PlayerKey player{};
    std::string player_name{};
    DuelEvent event{};
    Alert alert{Alert::Unknown};
    Pattern pattern{Pattern::None};
    float anomaly_score{};
    float enemy_yaw{};
    float camera_yaw{};
    int health_before{-1};
    int health_after{-1};
    std::int64_t simulation_tick{};
    // False for all observed health changes: cause/attacker isn't known.
    bool attacker_confirmed{false};
};

struct PlayerCombatObservation {
    PlayerKey player{};
    std::string player_name{};
    double timestamp_sec{};
    std::int64_t simulation_tick{};
    int health{};
    float last_fired_time{};
    float enemy_yaw{};
    float camera_yaw{};
    Alert alert{Alert::Unknown};
    Pattern pattern{Pattern::None};
    float anomaly_score{};
};

class DuelHistory {
public:
    void observeEnemy(const PlayerCombatObservation& observation);
    void observeLocal(std::uint64_t pawn, int health, double timestamp_sec, float camera_yaw);
    [[nodiscard]] std::vector<DuelEventRecord> recent(std::size_t limit = 150) const;
    [[nodiscard]] std::string csv() const;
    void clear();

private:
    struct PlayerState {
        int health{-1};
        float last_fired_time{};
        std::int64_t simulation_tick{-1};
        double timestamp_sec{-1.0};
        Alert alert{Alert::Unknown};
        bool initialized{};
    };
    void push(DuelEventRecord event);

    mutable std::mutex mutex_{};
    std::unordered_map<PlayerKey, PlayerState, PlayerKeyHash> players_;
    std::deque<DuelEventRecord> events_{};
    std::uint64_t local_pawn_{};
    int local_health_{-1};
    double local_time_{-1.0};
};

[[nodiscard]] DuelHistory& duelHistory();
[[nodiscard]] std::string_view toString(DuelEvent event) noexcept;
} // namespace vesta_aa
