#!/usr/bin/env python3
"""Create curated source archives and checksums; never include local build data."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import tarfile
import zipfile


ROOT_FILES = [
    ".gitignore", "CMakeLists.txt", "README.md", "ARCHITECTURE.md", "FORMATS.md",
    "LICENSE", "COPYING", "CORRESPONDING_SOURCE.md", "CREDITS.md", "THIRD_PARTY_NOTICES.md",
]
ROOT_DIRS = ["src", "tests", "scripts", "examples", "docs", "licenses"]


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dependencies", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    version = "1.0.0"
    args.output.mkdir(parents=True, exist_ok=True)
    source = args.output / f"ReliefForge-{version}-source.zip"
    dependencies = args.output / f"ReliefForge-{version}-dependency-sources.tar.gz"
    if source.exists() or dependencies.exists():
        parser.error("Source outputs already exist; archive them before making replacements.")

    manifest = json.loads((args.dependencies / "dependency-sources.json").read_text())
    if manifest["unmapped"]:
        raise RuntimeError("Cannot release with unmapped runtime dependencies")
    inputs = [{"path": package["archive"], "sha256": package["sha256"]}
              for package in manifest["packages"]]
    inputs.extend({"path": item["file"], "sha256": item["sha256"]}
                  for item in manifest["recipe_inputs"])
    for item in inputs:
        if digest(args.dependencies / item["path"]) != item["sha256"]:
            raise RuntimeError(f"Source checksum mismatch: {item['path']}")
    print(f"Verified {len(inputs)} upstream source and recipe inputs.", flush=True)

    source_files = [root / name for name in ROOT_FILES]
    source_files += [path for folder in ROOT_DIRS for path in (root / folder).rglob("*")
                    if path.is_file() and not path.is_symlink()
                    and ".DS_Store" not in path.parts and "__pycache__" not in path.parts
                    and path.suffix != ".pyc"]
    with zipfile.ZipFile(source, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(source_files):
            archive.write(path, Path(f"ReliefForge-{version}") / path.relative_to(root))
    with zipfile.ZipFile(source) as archive:
        if archive.testzip() is not None:
            raise RuntimeError("Source ZIP integrity failure")

    def clean_metadata(info: tarfile.TarInfo) -> tarfile.TarInfo:
        info.uid = info.gid = 0
        info.uname = info.gname = ""
        info.pax_headers = {}
        return info

    with tarfile.open(dependencies, "x:gz", compresslevel=1) as archive:
        for path in sorted(args.dependencies.rglob("*")):
            if not path.is_file() or path.is_symlink() or path.name == ".DS_Store":
                continue
            if path.suffix == ".partial":
                raise RuntimeError(f"Incomplete download: {path}")
            archive.add(path, arcname=Path(f"ReliefForge-{version}-dependency-sources") /
                        path.relative_to(args.dependencies), filter=clean_metadata)
        for name in ["docs/BUILDING_RELEASE.md", "CORRESPONDING_SOURCE.md", "COPYING", "THIRD_PARTY_NOTICES.md"]:
            archive.add(root / name, arcname=Path(f"ReliefForge-{version}-dependency-sources") / name,
                        filter=clean_metadata)
    assets = [args.output / f"ReliefForge-{version}-macOS-arm64.dmg",
              args.output / f"ReliefForge-{version}-macOS-arm64.zip", source, dependencies]
    lines = [f"{digest(path)}  {path.name}\n" for path in assets]
    (args.output / "CHECKSUMS-SHA256.txt").write_text("".join(lines))
    for path in assets:
        print(f"{path.name}: {path.stat().st_size:,} bytes", flush=True)


if __name__ == "__main__":
    main()
