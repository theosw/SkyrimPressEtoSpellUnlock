#include "arcane_activation/interaction.h"

namespace arcane_activation::interaction {
bool supports_arcane_unlock_target(const target_kind kind,
                                   const bool activation_blocked) noexcept {
  return kind != target_kind::unsupported && !activation_blocked;
}

bool same_target_family(const target_kind captured,
                        const target_kind current) noexcept {
  if (captured == target_kind::container) {
    return current == target_kind::container;
  }
  const bool captured_door =
      captured == target_kind::door || captured == target_kind::load_door;
  const bool current_door =
      current == target_kind::door || current == target_kind::load_door;
  return captured_door && current_door;
}

bool requires_post_unlock_reclose(const target_kind kind) noexcept {
  return kind == target_kind::load_door;
}

activation_action
choose_activation_action(const bool has_sufficient_spell,
                         const bool has_enough_magicka) noexcept {
  if (!has_sufficient_spell) {
    return activation_action::pass_through;
  }
  return has_enough_magicka ? activation_action::cast
                            : activation_action::suppress;
}

std::optional<lock_tier> tier_from_lock_bucket(const int bucket) noexcept {
  if (bucket < 0 || bucket > 4) {
    return std::nullopt;
  }
  return static_cast<lock_tier>(bucket + 1);
}

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
