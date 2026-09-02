#!/usr/bin/env python3
"""Preserve upstream notices and recipe inputs for a prepared source inventory.

Archives are inspected, not executed or extracted wholesale. Only regular,
bounded licence/notice files are copied under the requested notice directory.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import tarfile
from pathlib import Path, PurePosixPath


def fetch(url: str, path: Path, expected: str | None = None) -> dict:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        subprocess.run(["curl", "--fail", "--location", "--silent", "--show-error",
                        "--retry", "2", "--connect-timeout", "20", "--max-time", "180",
                        "--output", str(path), url], check=True)
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if expected and digest != expected:
        raise RuntimeError(f"Hash mismatch for {path}")
    return {"url": url, "sha256": digest, "file": str(path)}


def preserve_notices(archive: Path, target: Path) -> int:
    count = 0
    with tarfile.open(archive) as source:
        for member in source:
            parts = PurePosixPath(member.name).parts
            if not member.isfile() or member.size > 2_000_000 or len(parts) < 2 or ".." in parts:
                continue
            name = parts[-1].lower()
            legal = (any(word in name for word in ("license", "licence", "copying", "copyright", "notice", "exception"))
                     or any(part.lower() in ("licenses", "licences") for part in parts)
                     or name in ("ftl.txt", "gplv2.txt", "gplv3.txt", "lgplv21.txt", "lgplv3.txt", "qt_attribution.json")
                     or (name.startswith("readme") and (len(parts) == 2 or "3rdparty" in parts)))
            if not legal:
                continue
            stream = source.extractfile(member)
            if stream is None:
                continue
            data = stream.read()
            # Skip binary assets whose names happen to contain a matching word.
            if b"\0" in data:
                continue
            destination = target.joinpath(*parts[1:])
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
            count += 1
    return count


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sources", type=Path, required=True)
    parser.add_argument("--notices", type=Path, required=True)
    args = parser.parse_args()
    data = json.loads((args.sources / "dependency-sources.json").read_text())
    extra = []
    # Remote patches and resource archives are pinned by the installed recipe.
    for package in data["packages"]:
        recipe = (args.sources / package["recipe"]).read_text()
        blocks = re.findall(r'(?ms)^( +)(patch do|resource "[^"]+" do)\n(.*?)^\1end', recipe)
        for number, (_, kind, block) in enumerate(blocks, 1):
            if kind.startswith("resource") and kind != 'resource "gobject-introspection" do':
                continue  # Test fixtures and Linux-only test tools are not shipped.
            match = re.search(r'url "([^"]+)".*?sha256 "([a-f0-9]{64})"', block, re.S)
            if match:
                url, checksum = match.groups()
                filename = url.split("?", 1)[0].rsplit("/", 1)[-1]
                path = args.sources / "recipe-inputs" / package["name"] / f"{number}-{filename}"
                record = fetch(url, path, checksum)
                record["file"] = str(path.relative_to(args.sources))
                extra.append(record)
    # This file patch is referenced by the installed GLib recipe. Pin it to
    # the Homebrew commit recorded in the cached bottle manifest, not HEAD.
    glib_patch = "https://raw.githubusercontent.com/Homebrew/homebrew-core/b6db3dd411ecf3f7926395f1776107a07a0df9ad/Patches/glib/hardcoded-paths.diff"
    path = args.sources / "recipe-inputs/glib/Patches/glib/hardcoded-paths.diff"
    record = fetch(glib_patch, path)
    record["file"] = str(path.relative_to(args.sources))
    extra.append(record)
    # OCCT uses this header-only library; there is no standalone Mach-O to map.
    rapid = fetch("https://github.com/Tencent/rapidjson/archive/refs/tags/v1.1.0.tar.gz",
                  args.sources / "recipe-inputs/rapidjson/rapidjson-1.1.0.tar.gz",
                  "bf7ced29704a1e696fbccf2a2b4ea068e7774fa37f6d7dd4039d0787f8bed98e")
    rapid["file"] = str(Path(rapid["file"]).relative_to(args.sources))
    extra.append(rapid)
    record = fetch("https://github.com/Tencent/rapidjson/commit/9bd618f545ab647e2c3bcbf2f1d87423d6edf800.patch?full_index=1",
                   args.sources / "recipe-inputs/rapidjson/homebrew-backport.patch",
                   "ce341a69d6c17852fddd5469b6aabe995fd5e3830379c12746a18c3ae858e0e1")
    record["file"] = str(Path(record["file"]).relative_to(args.sources))
    extra.append(record)
    data["recipe_inputs"] = extra
    args.notices.mkdir(parents=True, exist_ok=True)
    total = 0
    for package in data["packages"]:
        archive = args.sources / package["archive"]
        if hashlib.sha256(archive.read_bytes()).hexdigest() != package["sha256"]:
            raise RuntimeError(f"Archive changed: {archive}")
        total += preserve_notices(archive, args.notices / package["name"])
        sbom_path = Path("/opt/homebrew/Cellar") / package["name"] / package["installed_version"] / "sbom.spdx.json"
        if sbom_path.exists():
            sbom = json.loads(sbom_path.read_text())
            package["upstream_license_expression"] = sbom["packages"][0]["licenseConcluded"]
        else:
            package["upstream_license_expression"] = "See upstream notices and installed formula recipe"
    total += preserve_notices(args.sources / rapid["file"], args.notices / "rapidjson")
    for record in extra:
        path = args.sources / record["file"]
        if path.name.endswith((".tar.xz", ".tar.gz", ".tgz", ".tar.bz2")) and "rapidjson" not in str(path):
            total += preserve_notices(path, args.notices / "build-resources" / path.name)
    shutil.copy2("/opt/homebrew/LICENSE.txt", args.sources / "recipes/HOMEBREW-LICENSE.txt")
    (args.notices / "homebrew").mkdir(exist_ok=True)
    shutil.copy2("/opt/homebrew/LICENSE.txt", args.notices / "homebrew/LICENSE.txt")
    (args.sources / "dependency-sources.json").write_text(json.dumps(data, indent=2) + "\n")
    summary = ["# Bundled dependency inventory", "", "Sources and build recipes are in the dependency-source release archive.", "",
               "| Component | Version | Upstream licence expression |", "|---|---|---|"]
    for package in data["packages"]:
        summary.append(f"| {package['name']} | {package['installed_version']} | {package['upstream_license_expression']} |")
    summary += ["", "The expressions above describe upstream packages, which can include tools and examples not shipped here.",
                "Original notices are preserved in the component subdirectories; source archives contain their full context.",
                "RapidJSON and recipe resource notices are also included."]
    (args.notices / "INDEX.md").write_text("\n".join(summary) + "\n")
    print(f"Preserved {total} upstream notice files and {len(extra)} pinned recipe inputs.")


if __name__ == "__main__":
    main()
