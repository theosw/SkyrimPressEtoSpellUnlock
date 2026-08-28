#include "pch.h"

#include "runtime_settings.h"

#include "activation.h"
#include "config.h"

namespace arcane_activation::runtime_settings {
namespace {
constexpr std::string_view papyrus_class = "ArcaneActivationNative";

std::int32_t get_charge_duration_ms(RE::StaticFunctionTag*) {
  return static_cast<std::int32_t>(activation::charge_duration_ms());
}

std::int32_t set_charge_duration_ms(RE::StaticFunctionTag*,
                                    const std::int32_t requested) {
  const auto applied = static_cast<std::uint32_t>(std::clamp(
      requested, static_cast<std::int32_t>(config::minimum_charge_duration_ms),
      static_cast<std::int32_t>(config::maximum_charge_duration_ms)));
  if (!config::write_charge_duration_ms(applied)) {
    const auto retained = get_charge_duration_ms(nullptr);
    logger::warn(
        "ARCANE_MCM_SETTING key=ChargeDurationMs, requested={}, applied={}, "
        "persisted=false",
        requested, retained);
    return retained;
  }

  activation::set_charge_duration_ms(applied);
  logger::info(
      "ARCANE_MCM_SETTING key=ChargeDurationMs, requested={}, applied={}, "
      "persisted=true",
      requested, applied);
  return static_cast<std::int32_t>(applied);
}

bool get_show_notifications(RE::StaticFunctionTag*) {
  return activation::show_notifications();
}

bool set_show_notifications(RE::StaticFunctionTag*, const bool requested) {
  if (!config::write_show_notifications(requested)) {
    const bool retained = get_show_notifications(nullptr);
    logger::warn(
        "ARCANE_MCM_SETTING key=ShowNotifications, requested={}, applied={}, "
        "persisted=false",
        requested, retained);
    return retained;
  }

  activation::set_show_notifications(requested);
  logger::info(
      "ARCANE_MCM_SETTING key=ShowNotifications, requested={}, applied={}, "
      "persisted=true",
      requested, requested);
  return requested;
}
} // namespace

bool register_papyrus(RE::BSScript::IVirtualMachine* virtual_machine) {
  if (virtual_machine == nullptr) {
    return false;
  }

  virtual_machine->RegisterFunction("GetChargeDurationMs", papyrus_class,
                                    &get_charge_duration_ms);
  virtual_machine->RegisterFunction("SetChargeDurationMs", papyrus_class,
                                    &set_charge_duration_ms);
  virtual_machine->RegisterFunction("GetShowNotifications", papyrus_class,
                                    &get_show_notifications);
  virtual_machine->RegisterFunction("SetShowNotifications", papyrus_class,
                                    &set_show_notifications);
  logger::info("ARCANE_MCM_NATIVE_REGISTERED class={}", papyrus_class);
  return true;
}
} // namespace arcane_activation::runtime_settings
