#include "arcane_activation/interaction.h"
#include "arcane_activation/timing.h"

#include <catch2/catch_test_macros.hpp>

namespace arcane_activation::interaction {
TEST_CASE("containers and doors are eligible arcane unlock targets") {
  CHECK(supports_arcane_unlock_target(target_kind::container, false));
  CHECK(supports_arcane_unlock_target(target_kind::door, false));
  CHECK(supports_arcane_unlock_target(target_kind::load_door, false));
  CHECK_FALSE(
      supports_arcane_unlock_target(target_kind::unsupported, false));
}

TEST_CASE("activation-blocked targets keep their normal behavior") {
  CHECK_FALSE(supports_arcane_unlock_target(target_kind::container, true));
  CHECK_FALSE(supports_arcane_unlock_target(target_kind::door, true));
  CHECK_FALSE(supports_arcane_unlock_target(target_kind::load_door, true));
}

TEST_CASE("captured doors remain compatible with either door classification") {
  CHECK(same_target_family(target_kind::container, target_kind::container));
  CHECK(same_target_family(target_kind::door, target_kind::door));
  CHECK(same_target_family(target_kind::door, target_kind::load_door));
  CHECK(same_target_family(target_kind::load_door, target_kind::door));
  CHECK_FALSE(same_target_family(target_kind::container, target_kind::door));
  CHECK_FALSE(same_target_family(target_kind::door, target_kind::container));
  CHECK_FALSE(
      same_target_family(target_kind::unsupported, target_kind::unsupported));
}

TEST_CASE("doors stay closed after Requiem unlocks them") {
  CHECK_FALSE(requires_post_unlock_reclose(target_kind::container));
  CHECK(requires_post_unlock_reclose(target_kind::door));
  CHECK(requires_post_unlock_reclose(target_kind::load_door));
  CHECK_FALSE(requires_post_unlock_reclose(target_kind::unsupported));
}

TEST_CASE("insufficient magicka consumes a valid arcane unlock attempt") {
  CHECK(choose_activation_action(false, false) ==
        activation_action::pass_through);
  CHECK(choose_activation_action(false, true) ==
        activation_action::pass_through);
  CHECK(choose_activation_action(true, false) == activation_action::suppress);
  CHECK(choose_activation_action(true, true) == activation_action::cast);
}

TEST_CASE("suppressed activation releases clear even while the game is paused") {
  CHECK(choose_suppressed_event_action(false, false, false, false) ==
        suppressed_event_action::pass_through);
  CHECK(choose_suppressed_event_action(true, false, true, true) ==
        suppressed_event_action::pass_through);
  CHECK(choose_suppressed_event_action(true, true, false, false) ==
        suppressed_event_action::consume);
  CHECK(choose_suppressed_event_action(true, true, false, true) ==
        suppressed_event_action::pass_through);
  CHECK(choose_suppressed_event_action(true, true, true, false) ==
        suppressed_event_action::consume_and_clear);
  CHECK(choose_suppressed_event_action(true, true, true, true) ==
        suppressed_event_action::consume_and_clear);
}

TEST_CASE("runtime lock buckets exclude unlocked and key-required locks") {
  CHECK(tier_from_lock_bucket(0) == lock_tier::novice);
  CHECK(tier_from_lock_bucket(1) == lock_tier::apprentice);
  CHECK(tier_from_lock_bucket(2) == lock_tier::adept);
  CHECK(tier_from_lock_bucket(3) == lock_tier::expert);
  CHECK(tier_from_lock_bucket(4) == lock_tier::master);
  CHECK_FALSE(tier_from_lock_bucket(-1).has_value());
  CHECK_FALSE(tier_from_lock_bucket(5).has_value());
}

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
