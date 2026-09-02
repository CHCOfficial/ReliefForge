#!/usr/bin/env python3
"""Assemble and audit a self-contained, ad-hoc signed ReliefForge Mac bundle.

Uses an existing deployed Qt bundle as the template, but replaces the app and
legal documents and recursively fills missing runtime dependencies. Upstream
binaries are copied, never edited in their original installation.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
from pathlib import Path


def run(*args: str) -> str:
    return subprocess.check_output(args, text=True)


def copy_contents(source: Path | str, destination: Path | str) -> str:
    """Copy bytes and Unix mode without invoking macOS copyfile/xattr APIs."""
    source = Path(source)
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with source.open("rb") as reader, destination.open("wb") as writer:
        shutil.copyfileobj(reader, writer, length=1024 * 1024)
    shutil.copymode(source, destination)
    return str(destination)


def binaries(bundle: Path) -> list[Path]:
    result = []
    for path in bundle.rglob("*"):
        if path.is_file() and not path.is_symlink():
            with path.open("rb") as stream:
                if stream.read(4) in {b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe", b"\xca\xfe\xba\xbe"}:
                    result.append(path)
    return result


def dependencies(path: Path) -> list[str]:
    return [line.strip().split(" (compatibility")[0] for line in run("otool", "-L", str(path)).splitlines()[1:]]


def local_target(dependency: str, binary: Path, bundle: Path) -> Path | None:
    contents = bundle / "Contents"
    if dependency.startswith("@executable_path/"):
        return contents / "MacOS" / dependency.removeprefix("@executable_path/")
    if dependency.startswith("@loader_path/"):
        return binary.parent / dependency.removeprefix("@loader_path/")
    if dependency.startswith("@rpath/"):
        return contents / "Frameworks" / dependency.removeprefix("@rpath/")
    return None


def complete_runtime(bundle: Path, prefix: Path) -> None:
    frameworks = bundle / "Contents/Frameworks"
    # Each pass can discover additional transitive libraries from new copies.
    while True:
        added = False
        for binary in binaries(bundle):
            for dependency in dependencies(binary):
                if dependency.startswith(("/System/Library/", "/usr/lib/")):
                    continue
                target = local_target(dependency, binary, bundle)
                if target and target.is_file():
                    continue
                if dependency.startswith(str(prefix) + "/"):
                    origin = Path(dependency)
                elif target is not None:
                    if ".framework/" in str(target):
                        origin = prefix / "lib" / str(target).split("/Frameworks/", 1)[1]
                    else:
                        origin = prefix / "lib" / target.name
                else:
                    raise RuntimeError(f"Unsupported dependency {dependency} in {binary}")
                if not origin.is_file():
                    raise RuntimeError(f"Missing installed runtime dependency: {origin}")
                if ".framework/" in str(origin):
                    framework_name = next(p for p in origin.parts if p.endswith(".framework"))
                    relative = str(origin).split(framework_name + "/", 1)[1]
                    destination = frameworks / framework_name / relative
                    if not destination.exists():
                        root = Path(str(origin).split(framework_name + "/", 1)[0]) / framework_name
                        subprocess.run(["ditto", "--norsrc", "--noextattr", "--noacl",
                                        str(root), str(frameworks / framework_name)], check=True)
                        added = True
                    replacement = "@rpath/" + framework_name + "/" + relative
                else:
                    destination = frameworks / origin.name
                    if not destination.exists():
                        copy_contents(origin.resolve(), destination)
                        destination.chmod(0o755)
                        print(f"Bundled {origin.name}", flush=True)
                        added = True
                    replacement = "@rpath/" + origin.name
                if dependency != replacement:
                    subprocess.run(["install_name_tool", "-change", dependency, replacement, str(binary)], check=True)
            ids = run("otool", "-D", str(binary)).splitlines()[1:]
            if ids and binary.is_relative_to(frameworks):
                identity = "@rpath/" + str(binary.relative_to(frameworks))
                if ids[0] != identity:
                    subprocess.run(["install_name_tool", "-id", identity, str(binary)], check=True)
            load_commands = run("otool", "-l", str(binary))
            for rpath in re.findall(r"cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset", load_commands):
                if rpath.startswith(("/opt/", "/usr/local/", "/Users/", "/private/tmp/", "/tmp/")):
                    subprocess.run(["install_name_tool", "-delete_rpath", rpath, str(binary)], check=True)
        if not added:
            break
    executable = bundle / "Contents/MacOS/ReliefForge"
    if "path @executable_path/../Frameworks " not in run("otool", "-l", str(executable)):
        subprocess.run(["install_name_tool", "-add_rpath", "@executable_path/../Frameworks", str(executable)], check=True)


def audit(bundle: Path) -> None:
    all_binaries = binaries(bundle)
    for binary in all_binaries:
        for dependency in dependencies(binary):
            if dependency.startswith(("/System/Library/", "/usr/lib/")):
                continue
            target = local_target(dependency, binary, bundle)
            if target is None or not target.is_file() or not target.resolve().is_relative_to(bundle.resolve()):
                raise RuntimeError(f"Unresolved or external dependency: {binary}: {dependency}")
        load_commands = run("otool", "-l", str(binary))
        paths = re.findall(r"(?:path|name) (.*?) \(offset", load_commands)
        if any(p.startswith(("/opt/", "/usr/local/", "/Users/", "/private/tmp/", "/tmp/")) for p in paths):
            raise RuntimeError(f"Non-portable load command: {binary}")
    print(f"Audited {len(all_binaries)} Mach-O files: every non-system dependency resolves inside the bundle.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--template", type=Path)
    parser.add_argument("--built-app", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--prefix", type=Path, default=Path("/opt/homebrew"))
    parser.add_argument("--audit-only", action="store_true")
    args = parser.parse_args()
    if args.audit_only:
        audit(args.output)
        return
    if not args.template or not args.built_app or args.output.exists():
        parser.error("Provide a template, built app and a new output path (existing outputs are not overwritten).")
    root = Path(__file__).resolve().parents[1]
    # Do not copy machine-local provenance/ACL metadata into the distribution.
    subprocess.run(["ditto", "--norsrc", "--noextattr", "--noacl",
                    str(args.template), str(args.output)], check=True)
    for path in args.output.rglob("*"):
        if not path.is_symlink():
            path.chmod(path.stat().st_mode | 0o200)
    for relative in ["Contents/MacOS/ReliefForge", "Contents/Info.plist"]:
        copy_contents(args.built_app / relative, args.output / relative)
    resources = args.output / "Contents/Resources"
    for name in ["LICENSE", "COPYING", "CREDITS.md", "THIRD_PARTY_NOTICES.md", "CORRESPONDING_SOURCE.md"]:
        copy_contents(root / name, resources / name)
    # Release files can carry machine-local provenance xattrs. rsync's standard
    # archive mode does not copy those macOS attributes and efficiently skips
    # the hundreds of unchanged upstream notices already in the template.
    subprocess.run(["rsync", "-a", str(root / "licenses") + "/",
                    str(resources / "licenses") + "/"], check=True)
    copy_contents(root / "src/app/assets/ReliefForge.icns", resources / "ReliefForge.icns")
    complete_runtime(args.output, args.prefix)
    audit(args.output)
    subprocess.run(["codesign", "--force", "--deep", "--sign", "-", str(args.output)], check=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(args.output)], check=True)
    print(args.output)


if __name__ == "__main__":
    main()
