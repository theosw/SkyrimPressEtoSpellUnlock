#pragma once

#include <cstdint>
#include <string_view>

namespace arcane_activation::activation {
[[nodiscard]] bool install();
void recover_runtime_state(std::string_view reason);
[[nodiscard]] bool show_notifications();
void set_show_notifications(bool enabled);
[[nodiscard]] std::uint32_t charge_duration_ms();
void set_charge_duration_ms(std::uint32_t milliseconds);
}
