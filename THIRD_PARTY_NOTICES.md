# Third-party notices — ReliefForge 1.0.0

ReliefForge 1.0.0 is distributed under GPL-3.0-only. Bundled dependencies retain their own copyright notices, licence terms and exceptions. Unmodified upstream notices are included in `licenses/third-party/` in the source distribution and the app's `Contents/Resources/` directory. See `licenses/third-party/INDEX.md` for the version inventory.

## Qt 6.11.1

The interface uses Qt Base, Declarative (QML, Quick and Quick Controls), Quick 3D, Shader Tools, SVG and Image Formats, copyright The Qt Company and other contributors.

Qt Quick 3D is available under GPLv3 or a commercial licence. This release uses the GPLv3 route, not a commercial Qt licence. Other Qt modules offer LGPL/GPL alternatives; individual tools, examples and embedded third-party components have their own notices. A Qt tools exception does not make the Quick 3D runtime MIT-licensed.

See [Qt Quick 3D licences and attributions](https://doc.qt.io/qt-6/qtquick3d-index.html#licenses-and-attributions) and [Qt licensing](https://doc.qt.io/qt-6/licensing.html). Exact module sources and installed build recipes accompany this release.

## Open CASCADE Technology 7.9.3

STEP export uses Open CASCADE Technology under the GNU Lesser General Public License version 2.1 with the Open CASCADE exception. Its licence, exception and third-party notices are preserved in `licenses/third-party/opencascade/`. The corresponding-source archive also includes RapidJSON 1.1.0 headers and the published Homebrew backport patch used by the build recipe.

## oneTBB 2023.1.0

Open CASCADE uses oneTBB under the Apache License 2.0. Its copyright and notice files are included in `licenses/third-party/tbb/`.

## Runtime libraries and image codecs

The bundle also includes Brotli, D-Bus, double-conversion, FreeType, GLib, Graphite2, HarfBuzz, ICU, gettext's libintl, JasPer, libjpeg-turbo, Little CMS, XZ's liblzma, MD4C, libmng, PCRE2, libpng, WebP, libb2, libtiff, OpenSSL and zstd. Their original notices are included in the component subdirectories.

This software is based in part on the work of the Independent JPEG Group.

Portions of this software are copyright © The FreeType Project (https://freetype.org). All rights reserved. This distribution uses FreeType's FTL licence option.

For alternative-licensed components, this GPLv3 distribution uses D-Bus's GPLv3 option (GPL-2.0-or-later), Graphite2's LGPL-2.1-or-later option and zstd's BSD option. Package-level licence expressions in the inventory can also cover tools and examples that are not included in the application.

Homebrew build recipes are provided with Homebrew's BSD 2-Clause licence, preserved in `licenses/third-party/homebrew/LICENSE.txt` and alongside the recipes.

## Source and modifications

Download both `ReliefForge-1.0.0-source.zip` and `ReliefForge-1.0.0-dependency-sources.tar.gz` from the same release page as the Mac application. They contain app source, upstream dependency archives, build recipes, recipe inputs and packaging scripts. See `CORRESPONDING_SOURCE.md` and `docs/BUILDING_RELEASE.md` for details, including rebuilding and replacing dynamically linked libraries.

No third-party project endorses ReliefForge. Use of the app alone does not relicense your imported images or exported models.
