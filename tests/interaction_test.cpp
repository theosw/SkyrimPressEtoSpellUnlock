#include "arcane_activation/interaction.h"
#include "arcane_activation/timing.h"

#include <catch2/catch_test_macros.hpp>

namespace arcane_activation::interaction {
TEST_CASE("raw lock levels map to Magic Redone tiers") {
  CHECK(tier_from_lock_level(0) == lock_tier::novice);
  CHECK(tier_from_lock_level(1) == lock_tier::novice);
  CHECK(tier_from_lock_level(2) == lock_tier::apprentice);
  CHECK(tier_from_lock_level(25) == lock_tier::apprentice);
  CHECK(tier_from_lock_level(26) == lock_tier::adept);
  CHECK(tier_from_lock_level(50) == lock_tier::adept);
  CHECK(tier_from_lock_level(51) == lock_tier::expert);
  CHECK(tier_from_lock_level(75) == lock_tier::expert);
  CHECK(tier_from_lock_level(76) == lock_tier::master);
  CHECK(tier_from_lock_level(254) == lock_tier::master);
  CHECK_FALSE(tier_from_lock_level(-1).has_value());
  CHECK_FALSE(tier_from_lock_level(255).has_value());
}

TEST_CASE("selection uses the cheapest sufficient known spell") {
  const std::array known{true, false, true, false, true};
  REQUIRE(choose_spell(lock_tier::novice, known).has_value());
  CHECK(choose_spell(lock_tier::novice, known)->tier == lock_tier::novice);
  CHECK(choose_spell(lock_tier::apprentice, known)->tier == lock_tier::adept);
  CHECK(choose_spell(lock_tier::expert, known)->tier == lock_tier::master);
  CHECK(choose_spell(lock_tier::master, known)->index == 4);
}

TEST_CASE("selection fails when no known spell can open the lock") {
  const std::array known{true, true, false, false, false};
  CHECK_FALSE(choose_spell(lock_tier::adept, known).has_value());
}

TEST_CASE("unlock timing does not release the animation too early") {
  const auto instant = timing::make_schedule(0);
  CHECK(instant.unlock_after_ms == 0);
  CHECK(instant.animation_release_after_ms == 450);

  const auto short_delay = timing::make_schedule(200);
  CHECK(short_delay.unlock_after_ms == 200);
  CHECK(short_delay.animation_release_after_ms == 450);

  const auto default_delay = timing::make_schedule(450);
  CHECK(default_delay.unlock_after_ms == 450);
  CHECK(default_delay.animation_release_after_ms == 450);

  const auto long_delay = timing::make_schedule(1000);
  CHECK(long_delay.unlock_after_ms == 1000);
  CHECK(long_delay.animation_release_after_ms == 1000);
}
}
