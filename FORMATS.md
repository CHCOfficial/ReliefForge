# ReliefForge Output Formats

## STL

STL receives a triangulated physical solid, never only the visible top surface. The mesh includes top, side walls, and bottom; all triangles use outward winding. Export is rejected by default if edge-use and degeneracy checks do not describe a closed manifold.

Binary STL writes the 80-byte header, 32-bit triangle count, and 50-byte records. ASCII STL is also implemented. Coordinates are emitted in the selected project scale; the current UI and CLI operate in millimetres.

## STEP

STEP is generated only when ReliefForge is compiled with OpenCascade. It is not a renamed STL and is not a triangle-soup wrapper.

The dense height field is resampled according to an independent CAD quality, fitted to a C2 B-spline surface, bounded as a top face, joined to a bottom plane with ruled side faces, sewn into a shell, converted to an oriented solid, and validated by OpenCascade. AP203, AP214, and AP242 writer modes are available in the C++ API; AP242 is the default.

The exporter reports fitted grid dimensions and maximum deviation at fit samples. Future validation will sample between fit points and add adaptive multi-patch fitting for discontinuities and very complex reliefs.

## SVG

SVG contains XML `path` elements with a physical `width`, `height`, and matching `viewBox`. Paths carry `data-layer` and `data-height-mm` metadata. Contours are actual interpolated vector paths; they are not embedded raster images.

## DXF

DXF emits native `LWPOLYLINE` entities in a valid ENTITIES section. `$INSUNITS` is set to millimetres. Layer names include `OUTLINE` and `CONTOURS`; the data model already accepts `ENGRAVING`, `BORDER`, and `MASK` for future generators.

## Project format

`.reliefstudio` is a UTF-8, line-oriented, escaped key/value format with an explicit schema version. Unknown keys are ignored for forward evolution. A project created by a newer unsupported schema fails with an understandable error rather than silently discarding data.

The current schema stores source/depth-map references, processing parameters, physical relief values, geometry resolution, style, contour settings, and camera values. Embedded images, masks, UI state, and recovery metadata will be added with schema migration tests.

Built-in example projects use a stable `source.image=builtin:<example-id>` reference, such as `builtin:topographic-waves`. The desktop resolves it against its bundled example catalog, so these projects can be moved without a companion image. Unknown example IDs fail explicitly. Ordinary imported images still use filesystem references and must remain available. This does not change the key/value schema; older app builds without the example library cannot resolve built-in references.
