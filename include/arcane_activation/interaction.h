#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arcane_activation::interaction {
enum class target_kind : std::uint8_t {
  unsupported,
  container,
  door,
  load_door,
};

enum class activation_action : std::uint8_t {
  pass_through,
  suppress,
  cast,
};

enum class lock_tier : std::uint8_t {
  novice = 1,
  apprentice = 2,
  adept = 3,
  expert = 4,
  master = 5,
};

struct spell_choice {
  lock_tier tier;
  std::size_t index;
};

[[nodiscard]] bool supports_arcane_unlock_target(
    target_kind kind, bool activation_blocked) noexcept;
[[nodiscard]] bool same_target_family(target_kind captured,
                                      target_kind current) noexcept;
[[nodiscard]] bool requires_post_unlock_reclose(target_kind kind) noexcept;
[[nodiscard]] activation_action
choose_activation_action(bool has_sufficient_spell,
                         bool has_enough_magicka) noexcept;

[[nodiscard]] std::optional<lock_tier>
tier_from_lock_bucket(int bucket) noexcept;
[[nodiscard]] std::optional<lock_tier> tier_from_lock_level(int level) noexcept;
[[nodiscard]] std::optional<spell_choice>
choose_spell(lock_tier required, const std::array<bool, 5>& known) noexcept;
}
