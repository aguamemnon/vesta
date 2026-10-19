#include <foundation/math/camera_prediction.hpp>

#include <cassert>
#include <limits>
#include <chrono>

using namespace std::chrono_literals;
using foundation::camera_prediction_sample;
using foundation::camera_projection_predictor;

int main()
{
    using clock = camera_projection_predictor::clock;
    const auto t = clock::now();

    camera_projection_predictor prediction{};
    camera_prediction_sample initial{};
    initial.fov = 90.0f;
    initial.matrix[0][0] = 1.0f;
    assert(!prediction.observe(initial, t, 3.0f));

    auto moving = initial;
    moving.matrix[0][0] = 0.95f;
    moving.angles.y = 1.0f;
    const auto predicted = prediction.observe(moving, t + 6ms, 3.0f);
    assert(predicted);
    assert((*predicted)[0][0] < moving.matrix[0][0]);
    // Bounded to half one frame: 1 -> 0.95 cannot project beyond 0.925.
    assert((*predicted)[0][0] >= 0.925f - 0.0001f);

    auto reversal = moving;
    reversal.matrix[0][0] = 0.97f;
    reversal.angles.y = -1.0f;
    assert(!prediction.observe(reversal, t + 12ms, 3.0f));

    auto fov_change = reversal;
    fov_change.fov = 60.0f;
    fov_change.matrix[0][0] = 0.7f;
    assert(!prediction.observe(fov_change, t + 18ms, 3.0f));

    auto jump = fov_change;
    jump.angles.y = 70.0f;
    assert(!prediction.observe(jump, t + 24ms, 3.0f));

    prediction.reset();
    assert(!prediction.observe(initial, t + 30ms, 3.0f));

    // Stationary camera must not predict a moving matrix.
    assert(!prediction.observe(initial, t + 36ms, 3.0f));

    // Handle wraparound without falsely detecting a 359-degree jump.
    prediction.reset();
    auto almost_full_circle = initial;
    almost_full_circle.angles.y = 359.0f;
    prediction.observe(almost_full_circle, t + 42ms, 3.0f);
    auto wrapped = almost_full_circle;
    wrapped.angles.y = 0.0f;
    wrapped.matrix[0][0] = 0.99f;
    assert(prediction.observe(wrapped, t + 48ms, 3.0f));

    // Unsafe inputs must not propagate NaN into the projected matrix.
    prediction.reset();
    initial.matrix[0][0] = 1.0f;
    prediction.observe(initial, t + 54ms, 3.0f);
    auto invalid = initial;
    invalid.angles.y = 1.0f;
    invalid.matrix[0][0] = std::numeric_limits<float>::quiet_NaN();
    assert(!prediction.observe(invalid, t + 60ms, 3.0f));
}
