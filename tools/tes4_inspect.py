#!/usr/bin/env python3
"""Small read-only inspector for Bethesda TES4 plugin records.

This is intentionally narrow. It exposes the record FormIDs and subrecords used
to validate Arcane Activation's generated plugin without relying on a GUI.
"""

from __future__ import annotations

import argparse
import json
import struct
import zlib
from pathlib import Path


COMPRESSED_RECORD = 0x0004_0000


def u16(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def text(data: bytes) -> str:
    return data.rstrip(b"\0").decode("cp1252", errors="replace")


def iter_subrecords(payload: bytes):
    pos = 0
    extended_size = None
    while pos + 6 <= len(payload):
        signature = payload[pos : pos + 4].decode("ascii", errors="replace")
        size = u16(payload, pos + 4)
        pos += 6
        if signature == "XXXX":
            if size != 4 or pos + 4 > len(payload):
                raise ValueError("Malformed XXXX subrecord")
            extended_size = u32(payload, pos)
            pos += 4
            continue
        if extended_size is not None:
            size = extended_size
            extended_size = None
        if pos + size > len(payload):
            raise ValueError(f"Subrecord {signature} runs past record payload")
        yield signature, payload[pos : pos + size]
        pos += size


def iter_records(data: bytes, start: int = 0, end: int | None = None):
    if end is None:
        end = len(data)
    pos = start
    while pos + 24 <= end:
        signature = data[pos : pos + 4].decode("ascii", errors="replace")
        if signature == "GRUP":
            group_size = u32(data, pos + 4)
            if group_size < 24 or pos + group_size > end:
                raise ValueError(f"Malformed GRUP at 0x{pos:X}")
            yield from iter_records(data, pos + 24, pos + group_size)
            pos += group_size
            continue

        size = u32(data, pos + 4)
        flags = u32(data, pos + 8)
        form_id = u32(data, pos + 12)
        payload_start = pos + 24
        payload_end = payload_start + size
        if payload_end > end:
            raise ValueError(f"Record {signature} at 0x{pos:X} runs past container")
        payload = data[payload_start:payload_end]
        if flags & COMPRESSED_RECORD:
            expected_size = u32(payload)
            payload = zlib.decompress(payload[4:])
            if len(payload) != expected_size:
                raise ValueError(f"Bad decompressed size for {signature}:{form_id:08X}")
        yield signature, form_id, flags, list(iter_subrecords(payload))
        pos = payload_end


def serialise_subrecord(signature: str, data: bytes):
    if signature in {"EDID", "FULL", "DESC", "MAST"}:
        return text(data)
    if len(data) == 4:
        return {"hex": data.hex(), "u32": u32(data), "float": struct.unpack("<f", data)[0]}
    if len(data) == 8 and signature == "CNTO":
        return {"item": f"{u32(data):08X}", "count": u32(data, 4)}
    return data.hex()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("plugin", type=Path)
    parser.add_argument("--signature", action="append", default=[])
    parser.add_argument("--edid", default="")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    signatures = {value.upper() for value in args.signature}
    edid_filter = args.edid.casefold()
    results = []
    for signature, form_id, flags, subrecords in iter_records(args.plugin.read_bytes()):
        if signatures and signature not in signatures:
            continue
        edid = next((text(value) for name, value in subrecords if name == "EDID"), "")
        if edid_filter and edid_filter not in edid.casefold():
            continue
        results.append(
            {
                "signature": signature,
                "form_id": f"{form_id:08X}",
                "flags": f"{flags:08X}",
                "edid": edid,
                "subrecords": [
                    {"signature": name, "value": serialise_subrecord(name, value)}
                    for name, value in subrecords
                ],
            }
        )

    if args.json:
        print(json.dumps(results, indent=2))
    else:
        for record in results:
            print(f"{record['signature']}:{record['form_id']} {record['edid']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
