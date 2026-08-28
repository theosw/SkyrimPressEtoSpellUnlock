"""Validate the Arcane Activation Open Animation Replacer package."""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any, Iterator


EXPECTED_ANIMATIONS = {
    "1hm_shout_inhale.hkx",
    "mt_shout_exhale.hkx",
    "mt_shout_inhale.hkx",
    "sneak1hm_shout_exhale.hkx",
    "sneak1hm_shout_inhale.hkx",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def load_object(path: Path) -> dict[str, Any]:
    require(path.is_file(), f"missing config: {path}")
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), f"config must contain a JSON object: {path}")
    return value


def nested_objects(value: Any) -> Iterator[dict[str, Any]]:
    if isinstance(value, dict):
        yield value
        for child in value.values():
            yield from nested_objects(child)
    elif isinstance(value, list):
        for child in value:
            yield from nested_objects(child)


def animation_names(path: Path) -> set[str]:
    require(path.is_dir(), f"missing animation directory: {path}")
    files = [entry for entry in path.iterdir() if entry.is_file() and entry.suffix.lower() == ".hkx"]
    for entry in files:
        require(entry.stat().st_size > 0, f"empty animation file: {entry}")
    return {entry.name.lower() for entry in files}


def validate(root: Path) -> tuple[int, int, int]:
    require(root.is_dir(), f"OAR mod root does not exist: {root}")
    root_config = load_object(root / "config.json")
    require(isinstance(root_config.get("name"), str) and root_config["name"].strip(),
            "parent config must have a non-empty name")

    preset_entries = root_config.get("conditionPresets", [])
    require(isinstance(preset_entries, list), "conditionPresets must be an array")
    preset_names: list[str] = []
    for entry in preset_entries:
        require(isinstance(entry, dict), "every condition preset must be an object")
        name = entry.get("name")
        require(isinstance(name, str) and name.strip(), "every condition preset must have a name")
        preset_names.append(name)
    require(len(preset_names) == len(set(preset_names)), "condition preset names must be unique")

    submods = sorted(
        directory for directory in root.iterdir()
        if directory.is_dir() and (directory / "config.json").is_file()
    )
    require(submods, "parent config exists, but no configured OAR submods were found")

    animation_count = 0
    for submod in submods:
        config = load_object(submod / "config.json")
        require(isinstance(config.get("name"), str) and config["name"].strip(),
                f"submod config must have a non-empty name: {submod}")
        require(isinstance(config.get("priority"), int),
                f"submod priority must be an integer: {submod}")
        conditions = config.get("conditions")
        require(isinstance(conditions, list) and conditions,
                f"submod must have at least one condition: {submod}")

        for condition in nested_objects(conditions):
            condition_type = condition.get("condition")
            if condition_type == "PRESET":
                preset = condition.get("Preset")
                require(isinstance(preset, str) and preset in preset_names,
                        f"submod references undefined preset {preset!r}: {submod}")
            elif condition_type == "HasSpell":
                spell = condition.get("Spell")
                require(isinstance(spell, dict),
                        f"HasSpell must contain a Spell object: {submod}")
                require(spell.get("pluginName") == "ArcaneActivation.esp",
                        f"HasSpell must reference ArcaneActivation.esp: {submod}")
                form_id = spell.get("formID")
                require(isinstance(form_id, str),
                        f"HasSpell formID must be a hexadecimal string: {submod}")
                require(int(form_id, 16) == 0x801,
                        f"HasSpell must reference marker spell 0x801: {submod}")

        third_person = animation_names(submod / "animations")
        first_person = animation_names(submod / "_1stperson" / "animations")
        require(third_person == first_person,
                f"first- and third-person animation sets differ: {submod}")
        require(third_person == EXPECTED_ANIMATIONS,
                f"animation set does not match the expected shout replacements: {submod}")
        animation_count += len(third_person)

    return len(submods), len(preset_names), animation_count


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: validate_oar_package.py <OAR mod root>", file=sys.stderr)
        return 2
    try:
        submods, presets, animation_count = validate(Path(sys.argv[1]))
    except (AssertionError, json.JSONDecodeError, OSError, ValueError) as error:
        print(f"OAR package validation failed: {error}", file=sys.stderr)
        return 1
    print(
        "Validated ArcaneActivation OAR package: "
        f"{submods} submod(s), {presets} preset(s), "
        f"{animation_count} first-person and {animation_count} third-person animations"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
