#!/usr/bin/env python3
# Rasterize the SLEWFOLD plate. Copyright (c) 2026 Martial Systems LLC. All rights reserved.
#
# The approved drawing is SergeMiddleCombined/resources/slewfold_v6.svg (1600 by
# 960). Knobs are baked into that drawing. This script removes the knob groups,
# the LED groups, and the switches that move, then rasterizes with rsvg-convert
# at 1x and 2x. The COUPLE switch stays: it is not a parameter, and the lever
# stays at AC. Readout rects stay. They are empty hit marks.
#
# Toggles and LEDs are drawn live by the editor, so leaving them in the bitmap
# would paint a second lever and a lamp that cannot turn off.

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SVG = ROOT / "SergeMiddleCombined" / "resources" / "slewfold_v6.svg"
PLATE = ROOT / "SergeMiddleCombined" / "resources" / "slewfold_plate.png"
FILM_2X = ROOT / "SergeMiddleCombined" / "resources" / "knob_filmstrip@2x.png"
FILM_1X = ROOT / "SergeMiddleCombined" / "resources" / "knob_filmstrip.png"

KEEP_SWITCH = "switch_top_couple"


def _group_span(text: str, id_at: int) -> tuple[int, int]:
    start = text.rfind("<g", 0, id_at)
    if start < 0:
        raise ValueError("group start")
    i = start
    depth = 0
    while i < len(text):
        nxt_open = text.find("<g", i)
        nxt_close = text.find("</g>", i)
        if nxt_close < 0:
            raise ValueError("unclosed group")
        if nxt_open >= 0 and nxt_open < nxt_close:
            depth += 1
            i = nxt_open + 2
            continue
        depth -= 1
        i = nxt_close + 4
        if depth == 0:
            return start, i
    raise ValueError("walk")


def strip_svg(text: str) -> str:
    """Drop baked knobs, lamps, and moving switches. Keep jacks and COUPLE."""
    import re

    ids = re.findall(r'<g id="([^"]+)"', text)
    drop = []
    for gid in ids:
        if gid.startswith("knob_") and not gid.endswith("_readout"):
            drop.append(gid)
        elif gid.startswith("led_"):
            drop.append(gid)
        elif gid.startswith("switch_") and gid != KEEP_SWITCH:
            drop.append(gid)
    for gid in drop:
        token = f'id="{gid}"'
        at = text.find(token)
        if at < 0:
            continue
        a, b = _group_span(text, at)
        text = text[:a] + text[b:]
    left = re.findall(r'id="([^"]+)"', text)
    for gid in left:
        if gid.startswith("led_") or (gid.startswith("knob_") and not gid.endswith("_readout")):
            raise RuntimeError(f"{gid} survived")
        if gid.startswith("switch_") and gid != KEEP_SWITCH:
            raise RuntimeError(f"{gid} survived")
    if f'id="{KEEP_SWITCH}"' not in text:
        raise RuntimeError("COUPLE switch was removed")
    return text


def rasterize(svg_text: str, dest: Path, width: int) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    proc = subprocess.run(
        ["rsvg-convert", "-w", str(width), "-o", str(dest)],
        input=svg_text.encode("utf-8"),
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"rsvg-convert failed ({proc.returncode})")


def write_filmstrip() -> None:
    image = Image.open(FILM_2X).convert("RGBA")
    if image.size != (144, 64 * 144):
        raise RuntimeError(f"filmstrip is {image.size}, expected 144 by {64 * 144}")
    one = image.resize((72, 64 * 72), Image.Resampling.BOX)
    one.save(FILM_1X)


def main() -> int:
    text = SVG.read_text(encoding="utf-8")
    clean = strip_svg(text)
    rasterize(clean, PLATE, 1600)
    rasterize(clean, PLATE.with_name("slewfold_plate@2x.png"), 3200)
    write_filmstrip()
    for path, size in (
        (PLATE, (1600, 960)),
        (PLATE.with_name("slewfold_plate@2x.png"), (3200, 1920)),
        (FILM_1X, (72, 64 * 72)),
        (FILM_2X, (144, 64 * 144)),
    ):
        with Image.open(path) as im:
            if im.size != size:
                raise RuntimeError(f"{path.name} is {im.size}, expected {size}")
    print(f"wrote {PLATE.name} and knob filmstrips")
    return 0


if __name__ == "__main__":
    sys.exit(main())
