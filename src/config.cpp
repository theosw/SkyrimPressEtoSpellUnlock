#include "pch.h"

#include "config.h"

namespace arcane_activation::config {
namespace {
constexpr wchar_t section[] = L"Interaction";
constexpr wchar_t filename[] = L"Data\\SKSE\\Plugins\\ArcaneActivation.ini";

bool write_value(const wchar_t* key, const std::wstring& value) {
  if (WritePrivateProfileStringW(section, key, value.c_str(), filename) !=
      FALSE) {
    return true;
  }

  logger::error("ARCANE_CONFIG_WRITE_FAILED key={}, win32_error={}",
                std::filesystem::path(key).string(), GetLastError());
  return false;
}
}

t load() {
  const auto raw_charge_duration = static_cast<int>(GetPrivateProfileIntW(
      section, L"ChargeDurationMs",
      static_cast<int>(default_charge_duration_ms), filename));
  return {
      .show_notifications =
          GetPrivateProfileIntW(section, L"ShowNotifications", 1, filename) != 0,
      .charge_duration_ms = static_cast<std::uint32_t>(
          std::clamp(raw_charge_duration,
                     static_cast<int>(minimum_charge_duration_ms),
                     static_cast<int>(maximum_charge_duration_ms))),
  };
}

bool write_show_notifications(const bool enabled) {
  return write_value(L"ShowNotifications", enabled ? L"1" : L"0");
}

bool write_charge_duration_ms(const std::uint32_t milliseconds) {
  if (milliseconds < minimum_charge_duration_ms ||
      milliseconds > maximum_charge_duration_ms) {
    logger::warn("ARCANE_CONFIG_WRITE_REJECTED key=ChargeDurationMs, value={}",
                 milliseconds);
    return false;
  }
  return write_value(L"ChargeDurationMs", std::to_wstring(milliseconds));
}
}
