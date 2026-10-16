#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vesta_aa {

// Identity is scoped to a particular pawn/life, so recycled entity slots
// never inherit observations from a previous player.
struct PlayerKey {
    std::uint64_t id = 0;
    std::uint64_t generation = 0;
    bool operator==(const PlayerKey&) const = default;
};

struct PlayerKeyHash {
    std::size_t operator()(PlayerKey key) const noexcept;
};

struct OrientationSample {
    double timestamp_sec = 0.0;
    std::int64_t simulation_tick = 0;
    float eye_yaw_deg = 0.0f;
    float eye_pitch_deg = 0.0f;
    float velocity_units_s = 0.0f;
    bool angles_valid = false;
    bool simulated_update = false;
    bool fallback_angles = false;
    bool reused_bones = false;
};

enum class Pattern : std::uint8_t {
    None, JitterLike, SpinLike, AbruptChange
};

enum class Alert : std::uint8_t {
    Unknown, Normal, Suspicious, RepeatedAnomalies
};

struct Assessment {
    Pattern pattern = Pattern::None;
    Alert alert = Alert::Unknown;
    float anomaly_score = 0.0f; // 0..1, not a probability of cheating.
    std::size_t valid_samples = 0;
    int direction_reversals = 0;
    bool accepted = false;
};

struct AlertRecord {
    PlayerKey player{};
    double timestamp_sec{};
    std::int64_t simulation_tick{};
    Alert alert{Alert::Unknown};
    Pattern pattern{Pattern::None};
    float anomaly_score{};
};

class AntiAimObserver {
public:
    Assessment observe(PlayerKey key, const OrientationSample& sample);
    void forget(PlayerKey key);
    void clear();
    [[nodiscard]] std::size_t trackedPlayers() const noexcept;
    // Bounded diagnostics only; actual duel attribution needs a separate event source.
    [[nodiscard]] std::vector<AlertRecord> recentTransitions(std::size_t limit = 64) const;
    [[nodiscard]] std::vector<OrientationSample> recentSamples(PlayerKey key) const;

private:
    struct Track {
        std::deque<OrientationSample> samples;
        std::int64_t last_tick = -1;
        double last_time = -1.0;
        int consecutive_suspicious = 0;
        double suspicious_since = -1.0;
        Pattern last_pattern = Pattern::None;
        Assessment last_assessment{};
    };

    static Assessment assess(const Track& track);
    std::unordered_map<PlayerKey, Track, PlayerKeyHash> tracks_;
    std::deque<AlertRecord> transitions_;
};

std::string_view toString(Alert value) noexcept;
std::string_view toString(Pattern value) noexcept;

} // namespace vesta_aa
