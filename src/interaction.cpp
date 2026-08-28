#include "arcane_activation/interaction.h"

namespace arcane_activation::interaction {
std::optional<lock_tier> tier_from_lock_level(const int level) noexcept {
  if (level < 0 || level > 254) {
    return std::nullopt;
  }
  if (level <= 1) {
    return lock_tier::novice;
  }
  if (level <= 25) {
    return lock_tier::apprentice;
  }
  if (level <= 50) {
    return lock_tier::adept;
  }
  if (level <= 75) {
    return lock_tier::expert;
  }
  return lock_tier::master;
}

std::optional<spell_choice>
choose_spell(const lock_tier required,
             const std::array<bool, 5>& known) noexcept {
  const std::size_t first = static_cast<std::size_t>(required) - 1;
  for (std::size_t index = first; index < known.size(); ++index) {
    if (known[index]) {
      return spell_choice{
          .tier = static_cast<lock_tier>(index + 1),
          .index = index,
      };
    }
  }
  return std::nullopt;
}
}

