#!/usr/bin/env python3
"""Validate Arcane Activation's generated inert casting proxy plugin."""

from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path

from tes4_inspect import iter_records, text, u32


def values(subrecords, signature: str):
    return [value for name, value in subrecords if name == signature]


def first(subrecords, signature: str) -> bytes:
    found = values(subrecords, signature)
    if not found:
        raise AssertionError(f"Missing {signature} subrecord")
    return found[0]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("plugin", type=Path)
    args = parser.parse_args()

    records = list(iter_records(args.plugin.read_bytes()))
    tes4 = next(record for record in records if record[0] == "TES4")
    if not tes4[2] & 0x200:
        raise AssertionError("Plugin is not ESL flagged")

    masters = [text(value) for value in values(tes4[3], "MAST")]
    if "Requiem - Magic Redone.esp" not in masters:
        raise AssertionError("Plugin does not retain Magic Redone as a master")
    for requirement in ("SkyUI_SE.esp", "MCMHelper.esp"):
        if requirement not in masters:
            raise AssertionError(f"Plugin does not retain MCM requirement {requirement}")

    by_edid = {}
    for signature, form_id, flags, subrecords in records:
        edids = values(subrecords, "EDID")
        if edids:
            by_edid[text(edids[0])] = (signature, form_id, flags, subrecords)

    effect = by_edid["AA_ProxyEffect"]
    spell = by_edid["AA_ProxySpell"]
    cast_fx_effect = by_edid["AA_CastFXEffect"]
    cast_fx_spell = by_edid["AA_CastFXSpell"]
    cast_fx_art = by_edid["AA_AlterationGreenHandArt"]
    mcm_quest = by_edid["AA_MCMQuest"]
    if (
        effect[0] != "MGEF"
        or spell[0] != "SPEL"
        or cast_fx_effect[0] != "MGEF"
        or cast_fx_spell[0] != "SPEL"
        or cast_fx_art[0] != "ARTO"
        or mcm_quest[0] != "QUST"
    ):
        raise AssertionError("Arcane Activation records have unexpected signatures")
    if values(effect[3], "VMAD"):
        raise AssertionError("Proxy effect still has a gameplay script")

    if "SpellHotbar.esp" in masters:
        raise AssertionError("Generated plugin retains an unwanted Spell Hotbar master")

    mcm_vmad = first(mcm_quest[3], "VMAD")
    for script_name in (b"ArcaneActivationMCM", b"SKI_PlayerLoadGameAlias"):
        if script_name not in mcm_vmad:
            raise AssertionError(
                f"MCM quest VMAD is missing {script_name.decode('ascii')}"
            )
    if struct.pack("<I", mcm_quest[1]) not in mcm_vmad:
        raise AssertionError("MCM quest VMAD does not reference its own player alias")

    spit = first(spell[3], "SPIT")
    if u32(spit) != 0:
        raise AssertionError("Proxy spell does not have zero base cost")
    charge_time = struct.unpack_from("<f", spit, 12)[0]
    if not math.isclose(charge_time, 0.25, abs_tol=1e-6):
        raise AssertionError(f"Unexpected proxy charge time: {charge_time}")
    casting_type = u32(spit, 16)
    delivery = u32(spit, 20)
    if casting_type != 1 or delivery != 0:
        raise AssertionError(
            f"Proxy is not Fire-and-Forget/Self: cast={casting_type}, delivery={delivery}"
        )

    master_count = len(masters)
    own_effect_id = effect[1]
    linked_effect_id = u32(first(spell[3], "EFID"))
    if linked_effect_id != own_effect_id:
        raise AssertionError(
            f"Proxy spell links {linked_effect_id:08X}, expected {own_effect_id:08X}"
        )

    cast_fx_spit = first(cast_fx_spell[3], "SPIT")
    if u32(cast_fx_spit, 8) != 4:
        raise AssertionError("Casting-effect spell is not an ability")
    if u32(cast_fx_spit, 16) != 0 or u32(cast_fx_spit, 20) != 0:
        raise AssertionError("Casting-effect spell is not Constant Effect/Self")
    cast_fx_effect_ids = [u32(value) for value in values(cast_fx_spell[3], "EFID")]
    if cast_fx_effect_ids != [cast_fx_effect[1]]:
        raise AssertionError("Casting-effect ability does not own exactly one effect")
    cast_fx_efit = first(cast_fx_spell[3], "EFIT")
    if u32(cast_fx_efit, 8) != 1:
        raise AssertionError("Casting-effect duration is not one second")
    cast_fx_data = first(cast_fx_effect[3], "DATA")
    if len(cast_fx_data) < 100 or u32(cast_fx_data, 96) != cast_fx_art[1]:
        raise AssertionError("Casting effect does not use its copied Alteration art")
    cast_fx_model = text(first(cast_fx_art[3], "MODL"))
    if cast_fx_model.lower() != "spellhotbar\\paralyzemasshandeffects_l.nif":
        raise AssertionError(f"Unexpected Alteration art model: {cast_fx_model}")
    own_records = [
        form_id & 0xFFFFFF
        for signature, form_id, _flags, _subrecords in records
        if signature != "TES4" and form_id >> 24 == master_count
    ]
    if len(own_records) != 6 or sorted(own_records) != [
        0x800,
        0x801,
        0x802,
        0x803,
        0x804,
        0x805,
    ]:
        raise AssertionError(
            f"Arcane Activation records are not the expected stable ESL forms: {own_records}"
        )

    print(
        f"Validated {args.plugin.name}: inert MGEF, zero-cost 0.25s self spell, "
        f"Spell Hotbar-compatible green cast FX, MCM quest, stable ESL forms, "
        f"{len(masters)} masters"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, KeyError, StopIteration, ValueError) as exc:
        print(f"Validation failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
