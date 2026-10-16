#include "anti_aim_observer.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace vesta_aa {
namespace {
constexpr std::size_t kWindow = 20;
constexpr std::size_t kMinSamples = 9;
constexpr float kSignificantStepDeg = 28.f;
constexpr float kSpinStepDeg = 12.f;
constexpr double kResetGapSec = 2.0;

float deltaYaw(float a, float b) {
    return std::remainder(b - a, 360.f);
}

bool finiteSample(const OrientationSample& s) {
    return std::isfinite(s.timestamp_sec) && std::isfinite(s.eye_yaw_deg) &&
           std::isfinite(s.eye_pitch_deg) && std::isfinite(s.velocity_units_s);
}
} // namespace

std::size_t PlayerKeyHash::operator()(PlayerKey key) const noexcept {
    std::size_t a = std::hash<std::uint64_t>{}(key.id);
    std::size_t b = std::hash<std::uint64_t>{}(key.generation);
    return a ^ (b + std::size_t{0x9e3779b9} + (a << 6) + (a >> 2));
}

Assessment AntiAimObserver::observe(PlayerKey key, const OrientationSample& s) {
    if (!finiteSample(s) || !s.angles_valid || !s.simulated_update ||
        s.fallback_angles || s.reused_bones || key.id == 0) {
        if (auto it = tracks_.find(key); it != tracks_.end()) {
            auto prior = it->second.last_assessment;
            prior.accepted = false;
            return prior;
        }
        return {};
    }
    // Keep the state bounded over long sessions.
    if (tracks_.size() >= 256 && !tracks_.contains(key)) tracks_.clear();
    auto& track = tracks_[key];
    if (track.last_time >= 0.0 &&
        (s.timestamp_sec - track.last_time > kResetGapSec ||
         s.timestamp_sec < track.last_time || s.simulation_tick < track.last_tick)) {
        track = Track{};
    }
    if (s.simulation_tick <= track.last_tick || s.timestamp_sec <= track.last_time) {
        auto prior = track.last_assessment;
        prior.accepted = false;
        return prior;
    }
    track.last_tick = s.simulation_tick;
    track.last_time = s.timestamp_sec;
    track.samples.push_back(s);
    if (track.samples.size() > kWindow) track.samples.pop_front();
    auto result = assess(track);
    result.accepted = true;

    if (result.pattern != Pattern::None && result.anomaly_score >= 0.63f) {
        track.consecutive_suspicious = track.last_pattern == result.pattern
            ? std::min(1000, track.consecutive_suspicious + 1) : 1;
        if (track.last_pattern != result.pattern || track.suspicious_since < 0.0)
            track.suspicious_since = s.timestamp_sec;
        track.last_pattern = result.pattern;
        result.alert = track.consecutive_suspicious >= 8 &&
            (s.timestamp_sec - track.suspicious_since) >= 0.75
            ? Alert::RepeatedAnomalies : Alert::Suspicious;
    } else {
        track.consecutive_suspicious = std::max(0, track.consecutive_suspicious - 2);
        if (track.consecutive_suspicious == 0) {
            track.last_pattern = Pattern::None;
            track.suspicious_since = -1.0;
        }
        result.alert = track.samples.size() >= kMinSamples
            ? Alert::Normal : Alert::Unknown;
    }
    if (result.alert != track.last_assessment.alert) {
        transitions_.push_back({key, s.timestamp_sec, s.simulation_tick,
                                result.alert, result.pattern, result.anomaly_score});
        while (transitions_.size() > 256) transitions_.pop_front();
    }
    track.last_assessment = result;
    return result;
}

Assessment AntiAimObserver::assess(const Track& track) {
    Assessment out;
    out.valid_samples = track.samples.size();
    if (track.samples.size() < kMinSamples) return out;

    std::vector<float> steps;
    steps.reserve(track.samples.size() - 1);
    int significant = 0, flips = 0, positive = 0, negative = 0, fast = 0;
    int prior_dir = 0;
    for (std::size_t i = 1; i < track.samples.size(); ++i) {
        const float d = deltaYaw(track.samples[i - 1].eye_yaw_deg,
                                 track.samples[i].eye_yaw_deg);
        steps.push_back(d);
        if (d >= kSpinStepDeg) ++positive;
        if (d <= -kSpinStepDeg) ++negative;
        if (std::fabs(d) >= 65.f) ++fast;
        if (std::fabs(d) < kSignificantStepDeg) continue;
        ++significant;
        const int dir = d > 0.f ? 1 : -1;
        if (prior_dir && prior_dir != dir) ++flips;
        prior_dir = dir;
    }
    out.direction_reversals = flips;
    const auto intervals = static_cast<float>(steps.size());
    if (significant >= 6 && flips >= 4) {
        const float reversal_ratio = static_cast<float>(flips) /
            static_cast<float>(std::max(1, significant - 1));
        const float activity_ratio = static_cast<float>(significant) / intervals;
        if (reversal_ratio >= 0.64f) {
            out.pattern = Pattern::JitterLike;
            out.anomaly_score = std::clamp(
                0.45f + 0.36f * reversal_ratio + 0.20f * activity_ratio, 0.f, 1.f);
            return out;
        }
    }
    if (std::max(positive, negative) >= 12 &&
        std::min(positive, negative) <= 2) {
        out.pattern = Pattern::SpinLike;
        out.anomaly_score = std::clamp(0.58f +
            0.32f * static_cast<float>(std::max(positive, negative)) / intervals,
            0.f, 1.f);
        return out;
    }
    if (fast >= 5 && significant >= 6) {
        out.pattern = Pattern::AbruptChange;
        out.anomaly_score = std::clamp(0.48f +
            0.4f * static_cast<float>(fast) / intervals, 0.f, 1.f);
    }
    return out;
}

Assessment AntiAimObserver::lastAssessment(PlayerKey key) const {
    if (const auto it = tracks_.find(key); it != tracks_.end())
        return it->second.last_assessment;
    return {};
}
void AntiAimObserver::forget(PlayerKey key) { tracks_.erase(key); }
void AntiAimObserver::clear() { tracks_.clear(); transitions_.clear(); }
std::size_t AntiAimObserver::trackedPlayers() const noexcept { return tracks_.size(); }
std::vector<AlertRecord> AntiAimObserver::recentTransitions(std::size_t limit) const {
    const auto n = std::min(limit, transitions_.size());
    return {transitions_.end() - static_cast<std::ptrdiff_t>(n), transitions_.end()};
}
std::vector<OrientationSample> AntiAimObserver::recentSamples(PlayerKey key) const {
    const auto it = tracks_.find(key);
    if (it == tracks_.end()) return {};
    return {it->second.samples.begin(), it->second.samples.end()};
}

std::string_view toString(Alert value) noexcept {
    switch (value) {
        case Alert::Unknown: return "unknown";
        case Alert::Normal: return "normal";
        case Alert::Suspicious: return "suspect";
        case Alert::RepeatedAnomalies: return "repeated_anomalies";
    }
    return "unknown";
}
std::string_view toString(Pattern value) noexcept {
    switch (value) {
        case Pattern::None: return "none";
        case Pattern::JitterLike: return "jitter_like";
        case Pattern::SpinLike: return "spin_like";
        case Pattern::AbruptChange: return "abrupt_changes";
    }
    return "none";
}
} // namespace vesta_aa
