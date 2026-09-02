#!/usr/bin/env python3
"""Inventory a Mac bundle against installed Homebrew binaries by Mach-O UUID.

Copies the exact installed formula recipes, records source URLs and hashes,
and optionally downloads hash-verified upstream source archives. Does not
execute upstream code or change the installed dependencies.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path


def command(*args: str) -> str:
    return subprocess.check_output(args, text=True, stderr=subprocess.DEVNULL)


def uuids(path: Path) -> set[str]:
    try:
        return set(re.findall(r"UUID: ([A-Fa-f0-9-]+)", command("dwarfdump", "--uuid", str(path))))
    except subprocess.CalledProcessError:
        return set()


def inventory(bundle: Path, cellar: Path, output: Path) -> dict:
    candidates = [p for p in bundle.rglob("*") if p.is_file() and not p.is_symlink()]
    binaries = []
    for path in candidates:
        with path.open("rb") as stream:
            magic = stream.read(4)
        if magic in {b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe", b"\xca\xfe\xba\xbe"}:
            binaries.append(path)
    names = {p.name for p in binaries if p.name != "ReliefForge"}
    installed: dict[str, list[Path]] = {}
    filenames = command("rg", "--files", "--hidden", "--no-ignore", str(cellar)).splitlines()
    # rg omits symlink files; versioned dylib aliases are how the bundle names
    # many libraries, so index those aliases against their resolved binaries.
    filenames += command("find", str(cellar), "-type", "l").splitlines()
    for filename in filenames:
        path = Path(filename)
        if path.name in names:
            installed.setdefault(path.name, []).append(path.resolve())
    packages: dict[str, dict] = {}
    unmapped = []
    for binary in sorted(binaries):
        if binary.name == "ReliefForge":
            continue
        identifiers = uuids(binary)
        matches = [p for p in installed.get(binary.name, []) if identifiers & uuids(p)]
        if not matches:
            unmapped.append(str(binary.relative_to(bundle)))
            continue
        origin = matches[0]
        package, version = origin.relative_to(cellar).parts[:2]
        key = package + "/" + version
        if key not in packages:
            prefix = cellar / package / version
            recipe = next((prefix / ".brew").glob("*.rb"))
            recipe_text = recipe.read_text()
            url_match = re.search(r'^  url "([^"]+)"', recipe_text, re.M)
            sha_match = re.search(r'^  sha256 "([a-f0-9]{64})"', recipe_text, re.M)
            if not url_match or not sha_match:
                raise RuntimeError(f"Review source URL/hash for {key}")
            receipt = json.loads((prefix / "INSTALL_RECEIPT.json").read_text())
            recipe_dir = output / "recipes" / package
            recipe_dir.mkdir(parents=True, exist_ok=True)
            shutil.copy2(recipe, recipe_dir / recipe.name)
            packages[key] = {
                "name": package, "installed_version": version,
                "source_url": url_match[1], "sha256": sha_match[1],
                "recipe": f"recipes/{package}/{recipe.name}",
                "compiler": receipt.get("compiler"),
                "bottle": receipt.get("poured_from_bottle"),
                "source_provenance": {k: v for k, v in receipt.get("source", {}).items() if k != "path"},
                "bundled_files": [],
            }
        packages[key]["bundled_files"].append({
            "path": str(binary.relative_to(bundle)),
            "uuid": sorted(identifiers),
            "origin": str(origin.relative_to(cellar)),
        })
    result = {"bundle": bundle.name, "mach_o_count": len(binaries),
              "packages": list(packages.values()), "unmapped": unmapped}
    (output / "dependency-sources.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def download(package: dict, output: Path) -> str:
    url = package["source_url"]
    suffix = ".tar.xz" if url.endswith(".tar.xz") else ".tar.bz2" if url.endswith(".tar.bz2") else ".tar.gz"
    name = package["name"].replace("@", "-") + "-" + package["installed_version"] + suffix
    path = output / "archives" / name
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or digest(path) != package["sha256"]:
        partial = path.with_name(path.name + ".partial")
        subprocess.run(["curl", "--fail", "--location", "--retry", "2",
                        "--connect-timeout", "20", "--max-time", "600",
                        "--silent", "--show-error", "--output", str(partial), url], check=True)
        if digest(partial) != package["sha256"]:
            raise RuntimeError(f"Source SHA-256 mismatch: {name}")
        partial.replace(path)
    package["archive"] = "archives/" + name
    package["bytes"] = path.stat().st_size
    return f"Verified {name} ({path.stat().st_size:,} bytes)"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path)
    parser.add_argument("--cellar", type=Path, default=Path("/opt/homebrew/Cellar"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--download", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    if args.bundle:
        data = inventory(args.bundle, args.cellar, args.output)
    else:
        data = json.loads((args.output / "dependency-sources.json").read_text())
    print(f"Mapped {data['mach_o_count'] - 1 - len(data['unmapped'])} dependency binaries to {len(data['packages'])} packages", flush=True)
    for package in data["packages"]:
        print(f"{package['name']} {package['installed_version']}: {package['source_url']}", flush=True)
    if data["unmapped"]:
        raise RuntimeError(f"Unmapped binaries: {data['unmapped']}")
    if args.download:
        failures = []
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            futures = {pool.submit(download, p, args.output): p for p in data["packages"]}
            for future in concurrent.futures.as_completed(futures):
                try:
                    print(future.result(), flush=True)
                except Exception as error:
                    failures.append(futures[future]["name"])
                    print(str(error), flush=True)
        (args.output / "dependency-sources.json").write_text(json.dumps(data, indent=2) + "\n")
        if failures:
            raise RuntimeError(f"Download failures: {failures}")


if __name__ == "__main__":
    main()
