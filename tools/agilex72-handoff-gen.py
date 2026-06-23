#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2026 Altera Corporation <www.altera.com>
#
# Handoff generator - Producer for the Agilex 72 HPS handoff
# binary blob.
#
# Emits the same wire format that the Quartus Assembler is expected
# to drop into SPL OCRAM on real silicon, as defined by KM_eSW_SAS
# tables 037-039 and the schema header at
# arch/arm/mach-socfpga/include/mach/agilex72-handoff.h.
#
# This script is the in-tree reference producer for the binary handoff
# format and is
# what generates the blob consumed by the SPL parser in
# drivers/clk/altera/agilex72-handoff-parser.c. For Simics pre-silicon
# use the --output-c-array form to embed the blob as a static const u8
# array into the SPL binary, since SDM / Quartus is not in the loop.
#
# Demo payload: V9 SYSPRESET0 bin1 (PLL presets, CLKMGR-top settings,
# HAS v0.83 default-divider CTRs, KV sequencing milestones). REG_ABS
# sections carry PLL presets and CLKMGR-top settings; KV_STRING sections
# carry sequencing milestones and per-output C-divider keys.
#
# Usage:
#   --output-bin agilex72-handoff.bin
#   --output-c-array agilex72-handoff-blob.c
#   --inspect      (dump human-readable form)
#
# The script is stdlib-only (no third-party deps) so it can run in
# CI / build hooks without an extra pip install step.

import argparse
import binascii
import struct
import sys
from dataclasses import dataclass, field
from typing import List, Optional


# --------------------------------------------------------------------------
# Format constants - keep in lock-step with
# arch/arm/mach-socfpga/include/mach/agilex72-handoff.h
# --------------------------------------------------------------------------

# "A72G" little-endian - chip-neutral public-facing magic for the
# Agilex 72 family. Replaces the early internal-codename
# sentinel ("HKM1" = 0x314D4B48). Keep in lock-step with
# AGILEX72_HANDOFF_MAGIC in
# arch/arm/mach-socfpga/include/mach/agilex72-handoff.h.
AGILEX72_HANDOFF_MAGIC = 0x47323741
AGILEX72_HANDOFF_VERSION_CURRENT = 1
# 32, not the 16 listed in eSW SAS table 039 - see agilex72-handoff.h.
AGILEX72_HANDOFF_KEY_MAX = 32

# enum agilex72_handoff_type
AGILEX72_HANDOFF_TYPE_KV = 1
AGILEX72_HANDOFF_TYPE_REG_ABS = 2
AGILEX72_HANDOFF_TYPE_KV_STRING = 3

# struct sizeofs (must match the C struct sizes - verified at runtime)
SZ_HANDOFF_HEADER = 20
SZ_HANDOFF_ENTRY = 16
SZ_KV_PAYLOAD_HEADER = 12
SZ_KV_ENTRY = 8
SZ_KV_STRING_ENTRY = 8
SZ_REG_ABS_PAYLOAD_HEADER = 16
SZ_REG_BIT_FIELD_ENTRY = 12

# Per the format convention all multi-byte fields are little-endian.
_PACK = "<"


def _pad_word(n: int) -> int:
    """Round n up to the next multiple of 4."""
    return (n + 3) & ~3


def _pad_bytes(buf: bytes) -> bytes:
    """Append zero bytes until len(buf) is word-aligned."""
    return buf + b"\x00" * (_pad_word(len(buf)) - len(buf))


# --------------------------------------------------------------------------
# Demo payload definitions — V9 SYSPRESET0 bin1 (Simics / pre-silicon).
# --------------------------------------------------------------------------

@dataclass
class RegAbsSection:
    """One REG_ABS subsystem block (=> one agilex72_reg_abs_payload_header)."""
    name: str
    base_address: int
    entries: List[tuple]  # list of (offset, mask, value)
    flags: int = 0
    payload_version: int = 1


@dataclass
class KvStringSection:
    """One KV_STRING block (=> one agilex72_kv_payload_header + entries)."""
    name: str
    pairs: List[tuple]  # list of (key, value)
    version: int = 1


def _abs_to_section(name: str, base: int, abs_pairs: List[tuple]) -> RegAbsSection:
    """Convert (abs_addr, value) tuples to (offset, mask=0xFFFFFFFF, value)
    relative to base. Raises on negative offsets so a wrong base is
    caught immediately.
    """
    entries = []
    for addr, value in abs_pairs:
        offset = addr - base
        if offset < 0:
            raise ValueError(
                f"{name}: addr 0x{addr:08X} < base 0x{base:08X}"
            )
        entries.append((offset, 0xFFFFFFFF, value))
    return RegAbsSection(name=name, base_address=base, entries=entries)


# PLL0 - peripheral GPPLL preset (SYSPRESET0 bin1 / 2000 MHz VCO).
_PLL0_BASE = 0x0915E000
_PLL0_ABS = [
    (0x0915E000, 0x00001CC0),
    (0x0915E004, 0x01400301),
    (0x0915E008, 0x06800210),
    (0x0915E00C, 0x01080700),
    (0x0915E010, 0x00205755),
    (0x0915E014, 0x02003400),
    (0x0915E018, 0x200E0080),
    (0x0915E01C, 0x008002FB),
    (0x0915E020, 0x00000048),
    (0x0915E024, 0x00800801),
    (0x0915E028, 0x01008004),
    (0x0915E02C, 0x00100702),
    (0x0915E030, 0x00120201),
    (0x0915E034, 0x00000000),
    (0x0915E038, 0x801A0201),
    (0x0915E03C, 0x001A0201),
    (0x0915E040, 0x0010A050),
    (0x0915E044, 0x00000000),
    (0x0915E048, 0x00380000),
    (0x0915E04C, 0x00000008),
    (0x0915E050, 0x00500000),
    (0x0915E054, 0x803A0101),
    (0x0915E058, 0x00000000),
    (0x0915E05C, 0x20000000),
    (0x0915E060, 0x00003800),
    (0x0915E064, 0x40000000),
]

# PLL1 - DSU GPPLL preset (1850 MHz VCO).
_PLL1_BASE = 0x0915F000
_PLL1_ABS = [
    (0x0915F000, 0x00001CC0),
    (0x0915F004, 0x01200301),
    (0x0915F008, 0x06800210),
    (0x0915F00C, 0x01080700),
    (0x0915F010, 0x00205755),
    (0x0915F014, 0x02003400),
    (0x0915F018, 0x200E0083),
    (0x0915F01C, 0x0081001B),
    (0x0915F020, 0x00000048),
    (0x0915F024, 0x00800901),
    (0x0915F028, 0x80804004),
    (0x0915F02C, 0x00180201),
    (0x0915F030, 0x00180201),
    (0x0915F034, 0x00000000),
    (0x0915F038, 0x00180201),
    (0x0915F03C, 0x00180201),
    (0x0915F040, 0x00180201),
    (0x0915F044, 0x00000000),
    (0x0915F048, 0x00380000),
    (0x0915F04C, 0x00000008),
    (0x0915F050, 0x04100000),
    (0x0915F054, 0x803A0101),
    (0x0915F058, 0x00000000),
    (0x0915F05C, 0x20800000),
    (0x0915F060, 0x00003800),
    (0x0915F064, 0x40000000),
]

# PLL2 - A720 core GPPLL preset (2500 MHz VCO).
_PLL2_BASE = 0x09160000
_PLL2_ABS = [
    (0x09160000, 0x00001CC0),
    (0x09160004, 0x01900301),
    (0x09160008, 0x06800EE8),
    (0x0916000C, 0x01080700),
    (0x09160010, 0x00205755),
    (0x09160014, 0x02003400),
    (0x09160018, 0x200E0083),
    (0x0916001C, 0x0081001B),
    (0x09160020, 0x00000048),
    (0x09160024, 0x00800901),
    (0x09160028, 0x80804004),
    (0x0916002C, 0x00180201),
    (0x09160030, 0x00180201),
    (0x09160034, 0x00000000),
    (0x09160038, 0x80180201),
    (0x0916003C, 0x00180201),
    (0x09160040, 0x00180201),
    (0x09160044, 0x00000000),
    (0x09160048, 0x00380000),
    (0x0916004C, 0x00000008),
    (0x09160050, 0x04100000),
    (0x09160054, 0x803A0101),
    (0x09160058, 0x00000000),
    (0x0916005C, 0x20000000),
    (0x09160060, 0x00003800),
    (0x09160064, 0x40000000),
]

# CLKMGR-top gate / bypass / ping-pong settings (V9 cookie words).
_CLKMGR_BASE = 0x0915C000
_CLKMGR_TOP_ABS = [
    (0x0915C030, 0x00000DE0),
    (0x0915C0D0, 0xFFFFFFFF),
    (0x0915C03C, 0x00000004),
    (0x0915C0F0, 0x00000000),
    (0x0915C0B4, 0x00000000),
    (0x0915C0BC, 0x00000000),
    (0x0915C0B8, 0x00000000),
    (0x0915C0C0, 0x00000000),
    (0x0915C0C4, 0x00000000),
]

# HAS v0.83 default-divider CTRs left at reset by the DV trace.
_CLKMGR_DIV_ABS = [
    (0x0915C050, 0x00800000),  # mainpllgrp.nocdiv apu_sysfreeclk=2 (div4)
    (0x0915C104, 0x00000000),
    (0x0915C108, 0x00000000),
    (0x0915C10C, 0x20002698),
    (0x0915C110, 0x00000000),
    (0x0915C130, 0x00000001),
    (0x0915C134, 0x00000001),  # gpiodbctr.cnt=1 ping-pong /2 (HAS Table 9-16)
    (0x0915C138, 0x00000000),
    (0x0915C13C, 0x00000000),
    (0x0915C140, 0x00000031),
    (0x0915C144, 0x00000000),
    (0x0915C148, 0x00000009),
    (0x0915C154, 0x00000009),
]


def build_demo_sections() -> List[object]:
    """
    Build the section list for the V9 SYSPRESET0 bin1 demo payload.
    """
    return [
        _abs_to_section("periph_pll0", _PLL0_BASE, _PLL0_ABS),
        _abs_to_section("dsu_pll1", _PLL1_BASE, _PLL1_ABS),
        _abs_to_section("a720_pll2", _PLL2_BASE, _PLL2_ABS),
        KvStringSection(
            name="pll_bringup",
            pairs=[("pll_enable", ""), ("pll_wait_lock", "")],
        ),
        KvStringSection(
            name="rate_state",
            pairs=[
                ("gppll0_c0_div", "2"),
                ("gppll0_c1_div", "4"),
                ("gppll0_c2_div", "5"),
                ("gppll0_c3_div", "1"),
                ("gppll1_c0_div", "1"),
                ("gppll1_c1_div", "1"),
                ("gppll2_c0_div", "1"),
                ("gppll2_c1_div", "1"),
            ],
        ),
        _abs_to_section("clkmgr_top", _CLKMGR_BASE, _CLKMGR_TOP_ABS),
        _abs_to_section("clkmgr_top_dividers", _CLKMGR_BASE, _CLKMGR_DIV_ABS),
        KvStringSection(
            name="exit_boot",
            pairs=[("boot_clk_bypass_disable", "")],
        ),
    ]


# --------------------------------------------------------------------------
# Serializers
# --------------------------------------------------------------------------

def _encode_reg_abs_section(s: RegAbsSection) -> bytes:
    """Encode one REG_ABS section to its on-wire byte image."""
    header = struct.pack(
        _PACK + "IIII",
        s.payload_version,
        s.base_address,
        len(s.entries),
        s.flags,
    )
    body = b"".join(
        struct.pack(_PACK + "III", off, mask, val)
        for (off, mask, val) in s.entries
    )
    return header + body


def _encode_kv_string_entry(key: str, value: str) -> bytes:
    """Encode one agilex72_kv_string_entry to bytes."""
    kb = key.encode("ascii")
    vb = value.encode("ascii")
    if len(kb) > AGILEX72_HANDOFF_KEY_MAX:
        raise ValueError(
            f"key {key!r} exceeds {AGILEX72_HANDOFF_KEY_MAX} bytes"
        )
    head = struct.pack(_PACK + "II", len(kb), len(vb))
    return head + _pad_bytes(kb) + _pad_bytes(vb)


def _encode_kv_string_section(s: KvStringSection) -> bytes:
    """Encode one KV_STRING section to its on-wire byte image."""
    body = b"".join(
        _encode_kv_string_entry(k, v) for (k, v) in s.pairs
    )
    total_length = SZ_KV_PAYLOAD_HEADER + len(body)
    header = struct.pack(
        _PACK + "III", s.version, len(s.pairs), total_length
    )
    return header + body


def _entry_type_for(section) -> int:
    if isinstance(section, RegAbsSection):
        return AGILEX72_HANDOFF_TYPE_REG_ABS
    if isinstance(section, KvStringSection):
        return AGILEX72_HANDOFF_TYPE_KV_STRING
    raise TypeError(f"unknown section type: {type(section).__name__}")


def build_blob(sections: List[object], crc32: bool = True) -> bytes:
    """
    Assemble the complete handoff blob.

    Layout matches the diagram in agilex72-handoff.h:
        header  |  entry[0..N-1]  |  payload[0..N-1]
    """
    # Encode each payload first so we know its length.
    payloads = []
    for s in sections:
        if isinstance(s, RegAbsSection):
            payloads.append(_encode_reg_abs_section(s))
        elif isinstance(s, KvStringSection):
            payloads.append(_encode_kv_string_section(s))
        else:
            raise TypeError(
                f"unsupported section: {type(s).__name__}"
            )

    n = len(sections)
    payload_off0 = SZ_HANDOFF_HEADER + n * SZ_HANDOFF_ENTRY

    entries_bytes = b""
    cur = payload_off0
    for sec, pld in zip(sections, payloads):
        if len(pld) % 4 != 0:
            raise ValueError(
                f"section {sec.name}: payload not word-aligned "
                f"({len(pld)} bytes)"
            )
        entries_bytes += struct.pack(
            _PACK + "IIII",
            _entry_type_for(sec),
            0,                  # flags
            len(pld) // 4,      # length in WORDS
            cur,                # data_offset from blob base
        )
        cur += len(pld)

    payload_bytes = b"".join(payloads)
    total_size = SZ_HANDOFF_HEADER + len(entries_bytes) + len(payload_bytes)

    # CRC32 is over [size_of_header, total_size). Compute over a
    # placeholder header (crc32=0) plus entries plus payloads, but
    # only the tail [sizeof(header):] per the spec.
    tail = entries_bytes + payload_bytes
    if crc32:
        crc = binascii.crc32(tail) & 0xFFFFFFFF
    else:
        crc = 0

    header_bytes = struct.pack(
        _PACK + "IIIII",
        AGILEX72_HANDOFF_MAGIC,
        AGILEX72_HANDOFF_VERSION_CURRENT,
        n,
        total_size,
        crc,
    )
    return header_bytes + tail


# --------------------------------------------------------------------------
# Output drivers
# --------------------------------------------------------------------------

def emit_bin(path: str, blob: bytes) -> None:
    with open(path, "wb") as f:
        f.write(blob)


def emit_c_array(path: str, blob: bytes, symbol: str = "agilex72_handoff_blob") -> None:
    """
    Emit a C source file that defines the blob as a static const u8 array
    plus a size variable. Suitable for linking into SPL when no real
    Quartus blob is present (Simics / emulation / pre-silicon).
    """
    lines = []
    # U-Boot / Linux convention: '//' SPDX for .c files, '/*' for .h.
    lines.append("// SPDX-License-Identifier: GPL-2.0")
    lines.append("/*")
    lines.append(" * AUTO-GENERATED by the Agilex 72 handoff generator")
    lines.append(" * DO NOT EDIT BY HAND - regenerate with:")
    lines.append(" *   --output-c-array <path>")
    lines.append(" */")
    lines.append("")
    lines.append("#include <linux/types.h>")
    lines.append("#include <linux/compiler.h>")
    lines.append("")
    lines.append(f"const u32 {symbol}_size = {len(blob)};")
    lines.append("")
    lines.append(f"const u8 __aligned(4) {symbol}[] = {{")
    for i in range(0, len(blob), 12):
        chunk = blob[i:i + 12]
        lines.append(
            "\t" + ", ".join(f"0x{b:02X}" for b in chunk) + ","
        )
    lines.append("};")
    lines.append("")
    with open(path, "w") as f:
        f.write("\n".join(lines))


def emit_inspect(blob: bytes, sections: List[object]) -> str:
    """Return a human-readable dump of the assembled blob."""
    out = []
    magic, version, entry_count, total_size, crc = struct.unpack(
        _PACK + "IIIII", blob[:SZ_HANDOFF_HEADER]
    )
    out.append(f"struct agilex72_handoff_header:")
    out.append(f"  magic       = 0x{magic:08X}")
    out.append(f"  version     = {version}")
    out.append(f"  entry_count = {entry_count}")
    out.append(f"  total_size  = {total_size} bytes")
    out.append(f"  crc32       = 0x{crc:08X}")
    out.append("")

    entries_off = SZ_HANDOFF_HEADER
    for i, s in enumerate(sections):
        eoff = entries_off + i * SZ_HANDOFF_ENTRY
        etype, flags, length, doff = struct.unpack(
            _PACK + "IIII", blob[eoff:eoff + SZ_HANDOFF_ENTRY]
        )
        type_name = {
            AGILEX72_HANDOFF_TYPE_KV: "KV",
            AGILEX72_HANDOFF_TYPE_REG_ABS: "REG_ABS",
            AGILEX72_HANDOFF_TYPE_KV_STRING: "KV_STRING",
        }.get(etype, f"?{etype}")
        out.append(f"entry[{i}] ({s.name}):")
        out.append(f"  type        = {type_name}")
        out.append(f"  flags       = 0x{flags:08X}")
        out.append(f"  length      = {length} words ({length * 4} bytes)")
        out.append(f"  data_offset = 0x{doff:04X} ({doff})")

        if isinstance(s, RegAbsSection):
            out.append(f"  base_address = 0x{s.base_address:08X}")
            out.append(f"  entries:")
            for off, mask, val in s.entries:
                addr = s.base_address + off
                out.append(
                    f"    +0x{off:03X} -> writel(0x{val:08X}, 0x{addr:08X}) "
                    f"mask=0x{mask:08X}"
                )
        elif isinstance(s, KvStringSection):
            out.append(f"  pairs:")
            for k, v in s.pairs:
                out.append(f"    {k!r} = {v!r}")
        out.append("")
    return "\n".join(out)


# --------------------------------------------------------------------------
# Entry point
# --------------------------------------------------------------------------

def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        description="Generate the Agilex 72 HPS handoff binary blob.",
    )
    ap.add_argument(
        "--output-bin",
        help="write raw .bin file (the format real Quartus emits)",
    )
    ap.add_argument(
        "--output-c-array",
        help="write a C source file with the blob as a static array",
    )
    ap.add_argument(
        "--symbol",
        default="agilex72_handoff_blob",
        help="C array symbol name (default: agilex72_handoff_blob)",
    )
    ap.add_argument(
        "--inspect",
        action="store_true",
        help="dump human-readable form of the blob to stdout",
    )
    ap.add_argument(
        "--no-crc32",
        action="store_true",
        help="zero the CRC32 field (default: compute and embed)",
    )
    args = ap.parse_args(argv)

    if not (args.output_bin or args.output_c_array or args.inspect):
        ap.error(
            "must specify at least one of --output-bin, "
            "--output-c-array, --inspect"
        )

    sections = build_demo_sections()
    blob = build_blob(sections, crc32=not args.no_crc32)

    if args.output_bin:
        emit_bin(args.output_bin, blob)
        print(
            f"wrote {len(blob)} bytes to {args.output_bin}",
            file=sys.stderr,
        )

    if args.output_c_array:
        emit_c_array(args.output_c_array, blob, symbol=args.symbol)
        print(
            f"wrote C array ({len(blob)} bytes -> {args.symbol}[]) "
            f"to {args.output_c_array}",
            file=sys.stderr,
        )

    if args.inspect:
        print(emit_inspect(blob, sections))

    return 0


if __name__ == "__main__":
    sys.exit(main())
