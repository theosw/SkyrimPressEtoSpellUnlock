#pragma once

namespace arcane_activation::config {
inline constexpr std::uint32_t default_charge_duration_ms = 450;
inline constexpr std::uint32_t minimum_charge_duration_ms = 0;
inline constexpr std::uint32_t maximum_charge_duration_ms = 1000;

struct t {
  bool show_notifications = true;
  std::uint32_t charge_duration_ms = default_charge_duration_ms;
};

[[nodiscard]] t load();
[[nodiscard]] bool write_show_notifications(bool enabled);
[[nodiscard]] bool write_charge_duration_ms(std::uint32_t milliseconds);
}
