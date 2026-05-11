#!/usr/bin/env python3
"""
Generate TrackMeta mapping snippet for W25Q128 PCM assets.

Usage:
  python tools/pcm_pack/pcm_pack.py \
    --name "Jiu Jiu Lao Qin" \
    --id 2 \
    --pcm "C:\\path\\song.pcm" \
    --addr 0x001000 \
    --sample-rate 22050 \
    --bits 16 \
    --channels 1
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path


def parse_u32(value: str) -> int:
    return int(value, 0)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", required=True)
    ap.add_argument("--id", type=int, required=True)
    ap.add_argument("--pcm", required=True)
    ap.add_argument("--addr", type=parse_u32, required=True)
    ap.add_argument("--sample-rate", type=int, default=22050)
    ap.add_argument("--bits", type=int, default=16)
    ap.add_argument("--channels", type=int, default=1)
    args = ap.parse_args()

    pcm_path = Path(args.pcm)
    if not pcm_path.exists():
        raise SystemExit(f"PCM file not found: {pcm_path}")

    data_len = os.path.getsize(pcm_path)
    bytes_per_sample = (args.bits // 8) * args.channels
    if bytes_per_sample <= 0:
        raise SystemExit("Invalid bits/channels")
    duration_ms = int((data_len * 1000) / (args.sample_rate * bytes_per_sample))

    print("/* Paste into src/music_library.c */")
    print("{")
    print(f'    {args.id}U, "{args.name}", {duration_ms}U, TRACK_TYPE_PCM,')
    print(f"    {args.sample_rate}U, {args.bits}U, {args.channels}U, 0x{args.addr:08X}U,")
    print(f"    (const void *)0, {data_len}U")
    print("},")
    print("")
    print("/* Summary */")
    print(f"file={pcm_path}")
    print(f"data_len={data_len} bytes")
    print(f"duration_ms={duration_ms}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

