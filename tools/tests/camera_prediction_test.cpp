#include <foundation/math/camera_prediction.hpp>

#include <cassert>
#include <chrono>
#include <cmath>
#include <limits>

using namespace std::chrono_literals;
using foundation::camera_prediction_sample;
using foundation::camera_projection_predictor;

namespace
{
camera_prediction_sample make_camera(float yaw_degrees, float eye_x = 0.0f, float fov = 90.0f)
{
    camera_prediction_sample v{};
    v.fov = fov;
    v.angles.y = yaw_degrees;
    v.origin.x = eye_x;
    const float angle = foundation::to_radians(yaw_degrees);
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    // Synthetic pinhole camera; three orthogonal axes (right, up, forward).
    v.matrix[0][0] = cosine;
    v.matrix[0][2] = -sine;
    v.matrix[0][3] = -cosine * eye_x;
    v.matrix[1][1] = 1.0f;
    v.matrix[2][0] = sine;
    v.matrix[2][2] = cosine;
    v.matrix[2][3] = -1.0f - sine * eye_x;
    v.matrix[3][0] = sine;
    v.matrix[3][2] = cosine;
    v.matrix[3][3] = -sine * eye_x;
    return v;
}

float dot_rows(const foundation::matrix4& m, int a, int b)
{
    return m[a][0] * m[b][0] + m[a][1] * m[b][1] + m[a][2] * m[b][2];
}

void approximately(float actual, float expected, float tolerance = 0.0001f)
{
    assert(std::abs(actual - expected) < tolerance);
}
} // namespace

int main()
{
    using clock = camera_projection_predictor::clock;
    const auto t = clock::now();
    camera_projection_predictor prediction{};
    const auto initial = make_camera(0);
    assert(!prediction.observe(initial, t, 3.0f));

    const auto moving = make_camera(1);
    const auto projected = prediction.observe(moving, t + 6ms, 3.0f);
    assert(projected);
    // Predict slightly further in the direction of rotation, not a linear
    // non-rigid interpolation of perspective matrix entries.
    assert((*projected)[3][0] > moving.matrix[3][0]);
    assert((*projected)[3][0] < std::sin(foundation::to_radians(1.51f)));
    approximately(dot_rows(*projected, 0, 1), 0);
    approximately(dot_rows(*projected, 0, 3), 0);
    approximately(dot_rows(*projected, 1, 3), 0);
    approximately(dot_rows(*projected, 3, 3), 1);
    approximately((*projected)[2][3] - (*projected)[3][3], -1);

    // Pure lateral camera movement advances camera-relative translation
    // without bending orthogonal axes.
    prediction.reset();
    assert(!prediction.observe(initial, t + 20ms, 3.0f));
    const auto walking = make_camera(0, 0.3f);
    const auto walking_projection = prediction.observe(walking, t + 26ms, 3.0f);
    assert(walking_projection);
    approximately((*walking_projection)[0][3], -0.45f);
    approximately(dot_rows(*walking_projection, 0, 3), 0);

    // Never chase an abrupt change of direction or a new FOV.
    prediction.reset();
    assert(!prediction.observe(initial, t + 40ms, 3.0f));
    assert(prediction.observe(make_camera(1), t + 46ms, 3.0f));
    assert(!prediction.observe(make_camera(-1), t + 52ms, 3.0f));
    assert(!prediction.observe(make_camera(-2, 0, 60), t + 58ms, 3.0f));
    assert(!prediction.observe(make_camera(70, 0, 60), t + 64ms, 3.0f));

    // Invalid and delayed snapshots must reset prediction rather than poison it.
    prediction.reset();
    assert(!prediction.observe(initial, t + 70ms, 3.0f));
    assert(!prediction.observe(make_camera(1), t + 120ms, 3.0f));
    auto invalid = make_camera(2);
    invalid.matrix[0][0] = std::numeric_limits<float>::quiet_NaN();
    assert(!prediction.observe(invalid, t + 126ms, 3.0f));
    assert(!prediction.observe(make_camera(3), t + 132ms, 3.0f));

    // Sample with new angles but a stale view matrix must be bypassed.
    prediction.reset();
    assert(!prediction.observe(initial, t + 140ms, 3.0f));
    auto stale = make_camera(1);
    stale.matrix = initial.matrix;
    assert(!prediction.observe(stale, t + 146ms, 3.0f));

    // Orientation near the 360-degree wrap is a small, not a 359-degree turn.
    prediction.reset();
    assert(!prediction.observe(make_camera(359), t + 160ms, 3.0f));
    assert(prediction.observe(make_camera(0), t + 166ms, 3.0f));
    prediction.reset();
    assert(!prediction.observe(initial, t + 170ms, 3.0f));
    assert(!prediction.observe(initial, t + 176ms, 3.0f));
}
