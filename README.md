# ReliefForge 1.0.0 — Image to fabrication

Turn a raster image into a 3D relief in one focused Mac workspace. ReliefForge 1.0.0 brings high-resolution smooth print previews, separate smooth/original STL exports, accurate height-curve graphs, seven material finishes, and clear, readable mesh statistics to the first public release.

ReliefForge is free and open source under GNU GPLv3. No subscriptions, locked exports or paid feature tiers.

## See it in action



## Start creating immediately

ReliefForge opens with **Topographic Waves** already loaded. Choose **Examples** for 12 offline starting points: ramps, domes, steps, geometric edges, waves, ripples, cushions, dunes, a spiral bloom, woven ribbons, ridged terrain and organic cells. Each example starts with clean default settings and can be edited, saved and exported like your own image. The picker reminds you to save any current work before replacing it. Example projects reopen without a separate image file; explicit image/project opens take priority over the startup demo.

## A considered first-run experience

- **Better defaults:** denoise/blur starts at **0**, geometry starts at **Ultra**, and smooth print preview remains enabled. Ultra uses up to 1024 samples along the source image's longest edge without upscaling the original field.
- **Accurate height-curve graph:** the plot now samples the same curve function used to generate relief heights. Linear is a straight diagonal; Soft, Strong, Bas Relief, and High Relief each display their real shape. Loaded custom curves are represented too.
- **Readable mesh counts:** vertices and triangles are displayed as full, grouped integers, such as **2,347,020**, never scientific notation. Formatting follows the system locale and retains full integer precision.
- **Working material presets:** Clay, Matte White, Aluminium, Bronze, Dark Metal, Wood, and Resin now update the model's color and finish immediately. These are visual presets, not physical material simulation; Wood is a wood-tone finish without a grain texture. Materials do not rebuild or alter export geometry.
- Existing projects retain their saved blur, resolution, and curve settings instead of being reset to the new-app defaults.

## Smooth preview and print exports

- **Smooth print preview** displays real, dense surface geometry, not just softened lighting. The preview and smooth STL writer share the same immutable mesh.
- **3D Print · Smooth High-Res STL…** exports that smooth mesh as a closed, manifold solid, with the physical footprint and base preserved.
- **3D Print · Original Geometry STL…** exports the canonical mesh unchanged, regardless of which preview is selected.
- Vertex and triangle counts follow the selected preview. Both export choices are always explicit.
- Smooth meshes are generated in the background; smooth STL writing is also asynchronous. Export is disabled while a newer preview is being prepared.
- Readable grouped mesh counts and a wider export menu make the print choices easier to inspect.

### How smooth printing works

The surface uses clamped bicubic interpolation with integer subdivision, preserving the original sample heights and height bounds. A 256 × 256 field becomes 766 × 766, producing **2,347,020 triangles** versus **262,140** in the original model. Subdivision normally stays within 512–1024 samples along the longest edge; already-dense source fields are retained without further subdivision or downsampling.

This adds smooth printable geometry between existing samples—it does not invent missing image detail. STL stores triangles, not the preview's material, lights, or smooth shading normals. Tests verify exact equality between the preview's float vertex positions and the STL vertex records. STEP, SVG, and DXF still use the original height field.

## Highlights

- Import PNG, JPEG, TIFF, BMP, and WebP images by dialog or drag and drop.
- Tune contrast, gamma, smoothing, inversion, physical width, relief depth, and base thickness with live rebuilds.
- Choose Standard, Inverted, Bas, High, Lithophane, Emboss, Deboss, Engraving, Contour, and Edge relief mappings.
- Inspect the generated relief in an interactive Qt Quick 3D viewport with perspective, front, and top views.
- Switch between the original faceted mesh and a high-resolution smooth print mesh.
- See exact dimensions, vertex and triangle counts, watertightness, and manifold status before export.
- Export smooth or original watertight STL, AP242 CAD STEP, SVG contours, and DXF contours.
- Save and reopen versioned `.reliefstudio` projects.
- Use the complete app without payment, subscriptions, locked exports, or feature gates.

After each successful image submission, a small dismissible toast offers optional creator support and social links. It never blocks processing or export and closes automatically.

## Downloads

- **Recommended:** `ReliefForge-1.0.0-macOS-arm64.dmg`
- **Portable alternative:** `ReliefForge-1.0.0-macOS-arm64.zip`
- **Application source:** `ReliefForge-1.0.0-source.zip`
- **Dependency source and build recipes:** `ReliefForge-1.0.0-dependency-sources.tar.gz`
- **Integrity:** `CHECKSUMS-SHA256.txt`

For the complete corresponding source of the bundled Mac app, download **both** source archives. The dependency archive is for developers/rebuilders; you do not need it just to run ReliefForge.

## Requirements

- Apple silicon Mac (arm64)
- macOS 26 or later
- No Homebrew or separate Qt/OpenCascade installation is required

This build currently requires macOS 26 because the packaged OpenCascade dependency was built with that deployment target.

## Install

1. Download and open the DMG.
2. Drag `ReliefForge.app` to the Applications shortcut.
3. Open ReliefForge from Applications.

This community build is ad-hoc signed and has not been notarized by Apple. If macOS blocks the first launch, first verify that the download is from a source you trust. Apple's documented per-app exception is available under **System Settings → Privacy & Security → Open Anyway** after the blocked launch. See [Apple's guidance for apps from unknown developers](https://support.apple.com/guide/mac-help/open-a-mac-app-from-an-unknown-developer-mh40616/26/mac/26). Developer ID signing and notarization remain future release work.

## Free software and creator credits

ReliefForge 1.0.0 is released under **GNU GPL version 3 only**, with the full licence, warranty disclaimer and source-download directions available in the app's **About** panel. Creator credits are informational and do not add restrictions to the licence:

- [Buy Me a Coffee](https://buymeacoffee.com/CHCOfficial)
- [Code](https://github.com/CHCOfficialGraphics)
- [Graphics](https://www.deviantart.com/chcofficialAudio)
- [Audio](https://suno.com/@artfulexpchc)

The app bundle and DMG include the GPLv3 licence, creator credits and original third-party notices. Qt Quick 3D is used under its GPLv3 licence; other dependencies retain their own licence terms and exceptions. Your imported images and exported models are not relicensed merely by using ReliefForge.

## Version 1 scope

This is a tested production-oriented vertical slice. Rectangular relief bases, iso-height vector contours, and a single fitted STEP surface are implemented. Masks and brush painting, non-rectangular/bevelled bases, adaptive mesh decimation, full undo/redo, autosave/recovery, and a broader cross-CAD validation matrix remain roadmap work.

## Verification

The native suite passes **34 tests**, covering startup examples and all 12 bundled maps, moved example projects, defaults, exact curve samples, loaded custom curves, 64-bit grouped counts, material selection, shared-mesh switching, exact preview/STL coordinate matching, watertight/manifold geometry, an OpenCascade STEP write/read round trip, and the embedded licence/source documents. The dependency-free core passes 20 tests separately. A **130-file Mach-O audit** checks that every non-system runtime dependency resolves inside the app. Third-party source archives are SHA-256 verified and mapped to bundled binaries by their Mach-O UUIDs. No physical 3D-printer test, full dependency rebuild or bit-for-bit reproducibility is claimed.
