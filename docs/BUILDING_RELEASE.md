# Building and modifying the Mac release

## Release environment

ReliefForge 1.0.0 was built for Apple Silicon (`arm64`) using Apple Clang 21, CMake, Ninja, Qt 6.11.1 and Open CASCADE 7.9.3. The shipped Homebrew runtime libraries require macOS 26; lowering the app's deployment target does not lower the libraries' minimum OS. Intel and older macOS builds are not supplied.

## Build ReliefForge

Install a compatible Apple SDK, C++20 compiler, CMake and Ninja. Make Qt and Open CASCADE available in your chosen prefix, then from the application source directory run:

```sh
cmake -S . -B build-native -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH='/opt/homebrew/opt/qt;/opt/homebrew/opt/opencascade'
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
```

Adapt the prefixes for your own installation. Check the configure output confirms that both the desktop application and genuine Open CASCADE STEP exporter are enabled. `README.md` also documents the portable-core build without these dependencies.

## Rebuild bundled dependencies

Download and extract `ReliefForge-1.0.0-dependency-sources.tar.gz` beside the application source archive. It contains:

- `archives/`: full upstream runtime source archives.
- `recipes/`: the exact Homebrew formula files stored with the installed runtime packages, plus Homebrew's licence.
- `recipe-inputs/`: pinned patches, RapidJSON's header-only source and the GLib introspection resource referenced by those recipes.
- `dependency-sources.json`: source URLs, SHA-256 digests, package versions, binary UUIDs and binary-to-package mappings.

For each dependency, unpack its source archive, apply the patches listed for it in the manifest, and follow the formula's `install` method and the upstream build instructions. Preserve the recipe's configure/CMake/Meson options, inline `inreplace` operations and any inline `__END__` patch. The GLib hardcoded-paths patch uses `@@HOMEBREW_PREFIX@@`; replace that placeholder with your chosen dependency prefix. Open CASCADE's recipe uses RapidJSON 1.1.0 with the supplied backport patch as a header-only build dependency. Build dependencies before their consumers.

These are the source inputs and recorded recipes for the installed Homebrew binaries; ReliefForge's executable was built locally against them. The release checks rebuild and test ReliefForge, but do not rebuild every third-party package from source and do not claim bit-for-bit reproducibility. Apple system frameworks, the macOS SDK and general-purpose build tools are not included.

## Bundle the app

Qt's `macdeployqt` can create an initial deployed Qt application bundle from a desktop build. For this release, an existing deployed ReliefForge bundle is used as a template, and the following script replaces the executable and metadata with the new build, refreshes notices, fills missing runtime libraries, rewrites load paths and ad-hoc signs the result:

```sh
python3 scripts/package_macos.py \
  --template /path/to/extracted/ReliefForge.app \
  --built-app build-native/ReliefForge.app \
  --output /path/to/new/ReliefForge.app
```

The output path must not exist. The default dependency prefix is `/opt/homebrew`; pass `--prefix` for another prefix. No installed library is modified. The audit requires every non-system Mach-O dependency to resolve inside the new bundle and rejects machine-specific load paths.

```sh
python3 scripts/package_macos.py --audit-only --output /path/to/new/ReliefForge.app
codesign --verify --deep --strict /path/to/new/ReliefForge.app
```

To use your own rebuilt Qt or other library, replace the corresponding framework or dylib in `Contents/Frameworks`, preserve or update its install names and dependencies, then ad-hoc sign your modified app with `codesign --force --deep --sign - /path/to/ReliefForge.app`. No activation secret, commercial Qt key or ReliefForge-specific signature is required. You are responsible for satisfying Apple's platform security requirements on your machine. Developer ID signing and notarization are separate processes; the supplied release is ad-hoc signed, not notarized.

## Prepare source and notice archives

`scripts/prepare_release_sources.py` inventories bundled binaries against installed Homebrew Mach-O UUIDs, preserves exact recipes and downloads hash-verified source archives. `scripts/complete_release_notices.py` adds pinned recipe inputs and extracts unmodified upstream licence and notice files. These scripts contain release-specific inputs; review them when changing dependency versions.

Publish the complete app-source ZIP and dependency-source TAR.GZ alongside the binary ZIP and DMG, freely downloadable from the same release page. Keep the full notices with any redistributed binary. `CORRESPONDING_SOURCE.md` explains this release's source-distribution method.
