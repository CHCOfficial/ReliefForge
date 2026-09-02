#!/usr/bin/env python3
"""Generate original procedural height maps, PNG thumbnails and the example catalog.

Uses only Python's standard library. Generated PNG/PGM assets are checked in;
running this script is not required to build or use the desktop application.
"""

from __future__ import annotations

import math
import json
import struct
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "examples"
SIZE = 256


def write_images(identifier: str, samples: list[int]) -> None:
    path = OUTPUT / (identifier + ".pgm")
    path.write_bytes(f"P5\n{SIZE} {SIZE}\n255\n".encode("ascii") + bytes(samples))
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    rows = b"".join(b"\0" + bytes(samples[y * SIZE:(y + 1) * SIZE]) for y in range(SIZE))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    (OUTPUT / (identifier + ".png")).write_bytes(png)


def sample_grid(function) -> list[int]:
    values: list[int] = []
    for y in range(SIZE):
        ny = y / (SIZE - 1)
        for x in range(SIZE):
            nx = x / (SIZE - 1)
            values.append(max(0, min(255, round(function(nx, ny) * 255))))
    return values


def main() -> None:
    OUTPUT.mkdir(exist_ok=True)
    tau = math.tau
    radius = lambda x, y: math.hypot(x - 0.5, y - 0.5)
    seeds = [(0.12, 0.18), (0.40, 0.10), (0.75, 0.16), (0.93, 0.40),
             (0.63, 0.45), (0.29, 0.43), (0.09, 0.72), (0.38, 0.83), (0.78, 0.80)]
    examples = [
        ("horizontal-gradient", "Horizontal Ramp", "Simple", "Learn how brightness becomes height.", lambda x, y: x),
        ("radial-dome", "Radial Dome", "Simple", "A rounded peak for trying height curves.",
         lambda x, y: max(0.0, 1.0 - radius(x, y) * 2.0)),
        ("terraced-steps", "Terraced Steps", "Simple", "Compare sharp steps with smooth printing.",
         lambda x, y: min(7, int(x * 8)) / 7),
        ("geometric-logo", "Geometric Cross", "Simple", "Bold edges for emboss and deboss.",
         lambda x, y: 1.0 if (abs(x - 0.5) < 0.08 or abs(y - 0.5) < 0.08 or abs(x - y) < 0.045) else 0.08),
        ("topographic-waves", "Topographic Waves", "Patterns", "The startup example. Try Contour Relief.",
         lambda x, y: 0.5 + 0.22 * math.sin(x * tau * 3 + math.sin(y * tau * 2)) + 0.12 * math.cos(y * tau * 5)),
        ("ripple-rings", "Ripple Rings", "Patterns", "Concentric ripples with soft highlights.",
         lambda x, y: 0.5 + 0.45 * math.cos(radius(x, y) * tau * 6)),
        ("soft-pillows", "Soft Pillows", "Patterns", "A grid of gently inflated cushions.",
         lambda x, y: 0.1 + 0.8 * (math.sin(x * math.pi * 4) ** 2 * math.sin(y * math.pi * 4) ** 2) ** 0.6),
        ("sculpted-dunes", "Sculpted Dunes", "Patterns", "Flowing sand-like ridges and valleys.",
         lambda x, y: 0.5 + 0.42 * math.sin(x * tau * 4 + 1.4 * math.sin(y * tau * 1.5))),
        ("spiral-bloom", "Spiral Bloom", "Intricate", "Six curling petals in a sculpted rosette.",
         lambda x, y: 0.08 + math.exp(-radius(x, y) ** 2 * 4) * (0.48 + 0.4 * math.cos(
             6 * math.atan2(y - 0.5, x - 0.5) + radius(x, y) * 28)) * min(1, radius(x, y) * 18)),
        ("woven-ribbons", "Woven Ribbons", "Intricate", "Interlaced waves for exploring finishes.",
         lambda x, y: 0.5 + 0.24 * math.sin(x * tau * 5 + 1.1 * math.sin(y * tau * 3))
         + 0.24 * math.cos(y * tau * 5 + 1.1 * math.sin(x * tau * 3))),
        ("ridged-terrain", "Ridged Terrain", "Intricate", "Layered mountain-like ridges and peaks.",
         lambda x, y: 0.08 + 0.48 * (1 - abs(math.sin(x * tau * 2 + math.cos(y * tau * 2))))
         + 0.24 * (1 - abs(math.sin(y * tau * 4 + math.sin(x * tau * 3))))
         + 0.12 * (1 - abs(math.sin((x + y) * tau * 8)))),
        ("organic-cells", "Organic Cells", "Intricate", "A field of softly rounded cellular peaks.",
         lambda x, y: 0.08 + 0.84 * math.exp(-min((x - sx) ** 2 + (y - sy) ** 2 for sx, sy in seeds) * 85)),
    ]
    catalog = []
    for identifier, name, level, description, function in examples:
        write_images(identifier, sample_grid(function))
        catalog.append(dict(id=identifier, name=name, level=level, description=description))
    (OUTPUT / "catalog.json").write_text(json.dumps(catalog, indent=2) + "\n")
    print(f"Generated {len(catalog)} procedural PNG/PGM examples in {OUTPUT}")


if __name__ == "__main__":
    main()
