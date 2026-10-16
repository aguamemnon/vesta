#include <features/visuals/duel_history.hpp>

#include <algorithm>
#include <cmath>
#include <format>

namespace vesta_aa {
namespace {
constexpr std::size_t kMaxPlayers = 256;
constexpr std::size_t kMaxEvents = 512;

std::string csvQuoted(const std::string& value) {
    std::string result = "\"";
    for (char c : value) {
        if (c == '"') result += "\"\"";
        else if (c == '\n' || c == '\r') result += ' ';
        else result += c;
    }
    result += '"';
    return result;
}

bool valid(const PlayerCombatObservation& s) {
    return s.player.id != 0 && s.player.generation != 0 &&
           s.simulation_tick > 0 && s.health >= 0 && s.health <= 100 &&
           std::isfinite(s.timestamp_sec) &&
           std::isfinite(s.enemy_yaw) && std::isfinite(s.camera_yaw) &&
           std::isfinite(s.last_fired_time) && std::isfinite(s.anomaly_score);
}
} // namespace

void DuelHistory::push(DuelEventRecord event) {
    events_.push_back(std::move(event));
    while (events_.size() > kMaxEvents) events_.pop_front();
}

void DuelHistory::observeEnemy(const PlayerCombatObservation& s) {
    if (!valid(s)) return;
    std::scoped_lock lock(mutex_);
    if (players_.size() >= kMaxPlayers && !players_.contains(s.player))
        players_.clear();
    auto& state = players_[s.player];
    if (!state.initialized || s.simulation_tick <= state.simulation_tick ||
        s.timestamp_sec <= state.timestamp_sec ||
        s.timestamp_sec - state.timestamp_sec > 3.0 ||
        (s.health > state.health + 40 && state.health >= 0)) {
        state = {s.health, s.last_fired_time, s.simulation_tick, s.timestamp_sec, s.alert, true};
        return;
    }

    const auto emit = [&](const DuelEvent kind, const int previous, const int current) {
        push(DuelEventRecord{s.timestamp_sec, s.player, s.player_name, kind,
                             s.alert, s.pattern, s.anomaly_score, s.enemy_yaw,
                             s.camera_yaw, previous, current, s.simulation_tick, false});
    };
    if (s.alert != state.alert &&
        (s.alert == Alert::Suspicious || s.alert == Alert::RepeatedAnomalies))
        emit(DuelEvent::AlertChanged, s.health, s.health);

    if (s.health < state.health) {
        emit(s.health == 0 ? DuelEvent::EnemyDied : DuelEvent::EnemyDamaged,
             state.health, s.health);
    }
    // Firing time is an observed game-state field, not a confirmed shot/hit.
    if (s.health > 0 && s.last_fired_time > state.last_fired_time + 0.0001f &&
        state.last_fired_time >= 0.0f)
        emit(DuelEvent::EnemyFired, s.health, s.health);

    state = {s.health, s.last_fired_time, s.simulation_tick, s.timestamp_sec, s.alert, true};
}

void DuelHistory::observeLocal(std::uint64_t pawn, int health, double timestamp_sec, float camera_yaw) {
    if (!pawn || health < 0 || health > 100 || !std::isfinite(timestamp_sec) ||
        !std::isfinite(camera_yaw)) return;
    std::scoped_lock lock(mutex_);
    if (pawn != local_pawn_ || local_health_ < 0 ||
        timestamp_sec <= local_time_ || timestamp_sec - local_time_ > 3.0 ||
        health > local_health_ + 40) {
        local_pawn_ = pawn;
        local_health_ = health;
        local_time_ = timestamp_sec;
        return;
    }
    if (health < local_health_) {
        push(DuelEventRecord{timestamp_sec, {}, "Local player",
                             health == 0 ? DuelEvent::LocalDied : DuelEvent::LocalDamaged,
                             Alert::Unknown, Pattern::None, 0.0f, 0.0f, camera_yaw,
                             local_health_, health, 0, false});
    }
    local_health_ = health;
    local_time_ = timestamp_sec;
}

std::vector<DuelEventRecord> DuelHistory::recent(std::size_t limit) const {
    std::scoped_lock lock(mutex_);
    auto n = std::min(limit, events_.size());
    return {events_.end() - static_cast<std::ptrdiff_t>(n), events_.end()};
}

std::string DuelHistory::csv() const {
    auto rows = recent(kMaxEvents);
    std::string result = "time_monotonic_sec,player_id,pawn,player_name,event,aa_status,aa_pattern,score,enemy_yaw,camera_yaw,hp_before,hp_after,simulation_tick,attacker_confirmed\n";
    for (const auto& row : rows) {
        result += std::format("{:.3f},{},{},{},{},{},{},{:.3f},{:.3f},{:.3f},{},{},{},{}\n",
                              row.recorded_at_sec, row.player.id, row.player.generation,
                              csvQuoted(row.player_name), toString(row.event), toString(row.alert),
                              toString(row.pattern), row.anomaly_score, row.enemy_yaw,
                              row.camera_yaw, row.health_before, row.health_after,
                              row.simulation_tick, row.attacker_confirmed ? "true" : "false");
    }
    return result;
}

void DuelHistory::clear() {
    std::scoped_lock lock(mutex_);
    players_.clear();
    events_.clear();
    local_pawn_ = 0;
    local_health_ = -1;
    local_time_ = -1.0;
}

DuelHistory& duelHistory() {
    static DuelHistory value;
    return value;
}

std::string_view toString(DuelEvent event) noexcept {
    switch (event) {
        case DuelEvent::EnemyFired: return "enemy_fired_observed";
        case DuelEvent::EnemyDamaged: return "enemy_damaged_unattributed";
        case DuelEvent::EnemyDied: return "enemy_died_unattributed";
        case DuelEvent::LocalDamaged: return "local_damaged_unattributed";
        case DuelEvent::LocalDied: return "local_died_unattributed";
        case DuelEvent::AlertChanged: return "orientation_alert_changed";
    }
    return "unknown";
}
} // namespace vesta_aa
