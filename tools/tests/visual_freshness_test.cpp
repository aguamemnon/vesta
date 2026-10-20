#include <game/state/pose_visual_freshness.hpp>

#include <chrono>
#include <cstdlib>

#define CHECK(condition) do { if (!(condition)) std::abort(); } while (false)

int main()
{
    using clock = game::detail::visual_clock;
    using namespace std::chrono_literals;
    const auto start = clock::now();
    const auto world = start;
    const auto bones = start;
    CHECK(!game::detail::visual_world_fresh({}, start));
    CHECK(!game::detail::visual_fallback_bone_fresh({}, start));
    CHECK(game::detail::visual_world_fresh(world, start));
    CHECK(game::detail::visual_world_fresh(world, start + 49ms));
    CHECK(game::detail::visual_world_fresh(world, start + 50ms));
    CHECK(!game::detail::visual_world_fresh(world, start + 51ms));
    CHECK(!game::detail::visual_world_fresh(start + 10ms, start));
    CHECK(game::detail::visual_fallback_bone_fresh(bones, start + 50ms));
    CHECK(!game::detail::visual_fallback_bone_fresh(bones, start + 51ms));
    CHECK(!game::detail::visual_timestamp_fresh(world, start + 101ms, 100ms));
    return 0;
}
