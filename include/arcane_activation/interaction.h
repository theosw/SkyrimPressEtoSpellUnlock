#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arcane_activation::interaction {
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

[[nodiscard]] std::optional<lock_tier> tier_from_lock_level(int level) noexcept;
[[nodiscard]] std::optional<spell_choice>
choose_spell(lock_tier required, const std::array<bool, 5>& known) noexcept;
}

