#!/usr/bin/env python3
"""Build a modern PNG-backed Apple ICNS container from an iconset directory."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


CHUNKS = (
    (b"icp4", "icon_16x16.png"),
    (b"icp5", "icon_32x32.png"),
    (b"icp6", "icon_32x32@2x.png"),
    (b"ic07", "icon_128x128.png"),
    (b"ic08", "icon_256x256.png"),
    (b"ic09", "icon_512x512.png"),
    (b"ic10", "icon_512x512@2x.png"),
)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("iconset", type=Path)
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()

    chunks: list[bytes] = []
    for kind, filename in CHUNKS:
        payload = (arguments.iconset / filename).read_bytes()
        chunks.append(kind + struct.pack(">I", len(payload) + 8) + payload)

    body = b"".join(chunks)
    arguments.output.write_bytes(b"icns" + struct.pack(">I", len(body) + 8) + body)
    print(f"Wrote {arguments.output} ({len(body) + 8:,} bytes)")


if __name__ == "__main__":
    main()
