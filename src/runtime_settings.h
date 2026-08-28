#pragma once

#include <RE/Skyrim.h>

namespace arcane_activation::runtime_settings {
[[nodiscard]] bool register_papyrus(
    RE::BSScript::IVirtualMachine* virtual_machine);
}
