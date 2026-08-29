#!/usr/bin/env python3
"""Check that every shipped unlock-delay default matches the native default."""

from __future__ import annotations

import configparser
import json
import re
import sys
from pathlib import Path


def read_ini_value(path: Path, key: str) -> int:
    parser = configparser.ConfigParser()
    parser.read(path, encoding="utf-8-sig")
    return parser.getint("Interaction", key)


def main() -> int:
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.cwd()
    header = (root / "src" / "config.h").read_text(encoding="utf-8")
    match = re.search(r"default_charge_duration_ms\s*=\s*(\d+)", header)
    if match is None:
        raise AssertionError("Native unlock-delay default was not found")

    values = {
        "native": int(match.group(1)),
        "plugin_ini": read_ini_value(
            root / "package" / "SKSE" / "Plugins" / "ArcaneActivation.ini",
            "ChargeDurationMs",
        ),
        "mcm_settings": read_ini_value(
            root
            / "package"
            / "MCM"
            / "Config"
            / "ArcaneActivation"
            / "settings.ini",
            "iChargeDurationMs",
        ),
    }
    mcm_config = json.loads(
        (
            root
            / "package"
            / "MCM"
            / "Config"
            / "ArcaneActivation"
            / "config.json"
        ).read_text(encoding="utf-8-sig")
    )
    slider = next(
        item
        for item in mcm_config["content"]
        if item.get("id") == "iChargeDurationMs:Interaction"
    )
    values["mcm_slider"] = int(slider["valueOptions"]["defaultValue"])

    if len(set(values.values())) != 1:
        detail = ", ".join(f"{name}={value}" for name, value in values.items())
        raise AssertionError(f"Unlock-delay defaults disagree: {detail}")

    print(f"Validated Arcane Activation unlock-delay defaults: {values['native']} ms")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, KeyError, StopIteration, ValueError) as exc:
        print(f"Validation failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
