#pragma once

#include <algorithm>
#include <cstdint>

namespace arcane_activation::timing {
inline constexpr std::uint32_t minimum_animation_release_ms = 450;

struct schedule {
  std::uint32_t unlock_after_ms;
  std::uint32_t animation_release_after_ms;
};

[[nodiscard]] constexpr schedule make_schedule(
    const std::uint32_t configured_unlock_delay_ms) {
  return {
      .unlock_after_ms = configured_unlock_delay_ms,
      .animation_release_after_ms =
          (std::max)(configured_unlock_delay_ms,
                     minimum_animation_release_ms),
  };
}
} // namespace arcane_activation::timing
