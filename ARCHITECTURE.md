# ReliefForge Architecture

## Design rule

The `HeightField` is the canonical representation. Export formats never convert into one another: the mesh, CAD, and vector branches independently consume the same physically scaled scalar field.

```text
decoded raster
    ↓
ImageProcessor (levels, contrast, gamma, smoothing, resampling)
    ↓
HeightCurve + relief style
    ↓
HeightField (millimetres)
    ├── MeshGenerator → MeshAnalyzer → STL
    ├── OCCT B-spline fitter → sewn B-rep solid → STEP
    └── marching squares → joined/simplified paths → SVG / DXF
```

## Modules

### Core

`GrayImage` stores normalised float samples without depending on a UI toolkit. `ImageProcessor` contains deterministic, testable operations. The desktop boundary converts Qt-decoded RGBA images into this type; the fallback CLI uses the compact PGM decoder.

`HeightCurve` maps normalised input to normalised height with ordered control points and linear or smooth interpolation. `HeightField` applies the curve and relief style, then stores final Z values in millimetres, including base thickness.

`ReliefPipeline` owns no global state. A parameter value completely determines its result, making background processing, caching, cancellation, CLI reuse, and future undo commands straightforward.

### Mesh

`MeshGenerator::rectangularSolid` builds shared indexed top and bottom grids and walks the perimeter counter-clockwise to add outward-facing side walls. The result is a closed volume. `MeshAnalyzer` counts undirected edge uses, degenerate triangles, exact duplicate vertices, boundary edges, non-manifold edges, and signed volume.

The current topology deliberately favours correctness and deterministic tests. Adaptive triangulation and topology-safe quadric decimation belong in separate stages rather than inside the base grid builder.

### CAD

The optional OCCT module samples the height field at an independently controlled STEP quality, fits a degree-3-to-8 C2 B-spline surface within a physical tolerance, builds the top face, creates ruled side faces, sews them to the bottom plane, orients the result, and rejects it unless `BRepCheck_Analyzer` validates the solid.

The next CAD milestone will split complex fields into a small number of adaptive patches, enforce compatible shared edges, and report deviation over denser validation samples. That avoids million-face triangle soup while retaining relief detail.

### Vector

The vector branch interpolates edge crossings using marching squares, resolves ambiguous saddle cells using the cell-centre value, joins segments using a physical tolerance, and simplifies paths with Ramer-Douglas-Peucker. Physical XY dimensions remain in millimetres.

### Desktop

`AppController` is the UI boundary and parameter model. It decodes images, debounces edits, computes on the Qt thread pool, ignores superseded revisions, publishes mesh statistics, and delegates export. `ReliefGeometry` converts the indexed CPU mesh into Quick 3D position/normal data. QML contains layout and presentation only.

Future command objects should mutate a project model, not geometry. Each command invalidates the earliest affected pipeline stage. Slider drags should coalesce from press to release.

## Performance direction

The implemented resolution presets already prevent source pixels from blindly becoming geometry. Planned caches are keyed by immutable parameter subsets:

1. decoded raster and image pyramid;
2. processed preview/full-resolution image;
3. height field;
4. mesh by sampling and base settings;
5. CAD fit by quality/tolerance;
6. vector paths by levels and simplification settings.

Workers should hold shared immutable buffers and publish complete results. Large images and meshes must not be copied into undo entries.

## Extension points

AI depth estimation, imported depth/normal/displacement maps, painted height layers, CNC toolpaths, and multi-material regions should all produce or modify height-field layers before representation-specific generation. They do not belong inside an exporter.

