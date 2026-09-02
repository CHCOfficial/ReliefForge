# Corresponding source for ReliefForge 1.0.0

The Mac binary distribution is accompanied by two source downloads on the same release page, available at no additional charge:

- `ReliefForge-1.0.0-source.zip` — the complete ReliefForge source, QML, assets, tests, CMake build files, licence notices and release scripts.
- `ReliefForge-1.0.0-dependency-sources.tar.gz` — hash-verified upstream source archives for bundled runtime dependencies, exact installed Homebrew formula recipes, provenance and a file-to-package inventory.

Download both source archives from the release page where you obtained the DMG or Mac ZIP. The small source ZIP alone is not the complete source set for the bundled desktop distribution. Publishers must attach both archives alongside the binaries and keep them available; do not substitute a generic upstream homepage or GitHub's automatically generated source ZIP for the dependency-source archive.

## Build and modification

See `README.md` for building the application and `docs/BUILDING_RELEASE.md` for the packaged runtime, dependency recipes, load-path changes and ad-hoc signing. Dependencies were taken from Homebrew bottles; their source archive hashes and exact installed formula recipes are captured in `dependency-sources.json` and `recipes/` in the dependency-source archive. Recipe patches and resources, when used, are included there as well.

No proprietary activation key is needed to build, modify or use ReliefForge. You can replace its executable or dynamic libraries and ad-hoc sign your modified copy. The release does not use DRM or an application-level signature check to prevent modified versions. Apple's ordinary platform security rules still apply.

The app's geometry, image-processing and export code can also be built without Qt or OpenCascade using the documented portable-core configuration. Apple system frameworks, the macOS SDK and general-purpose build tools are not included in the source archives.

## Notices

ReliefForge 1.0.0 is GPL-3.0-only. Individual third-party components retain their own licences and exceptions. See `LICENSE`, `COPYING`, `THIRD_PARTY_NOTICES.md` and `licenses/third-party/`. The full upstream source archives retain their original copyright and licence files.

The source publication method used for the downloadable Mac release is GPLv3 section 6(d): equivalent source access beside the binary downloads. This is not a promise that a source ZIP will be supplied only on request.
