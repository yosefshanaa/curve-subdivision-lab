# TODO — Subdivision Lab 3D

Ordered task list. **Milestones 0–3 are the deliverable.** Milestone 4 is polish
that was completed but was not required.

Every box below is checked because the work is done; the italic notes record how
each item was *verified*, not just that it was written.

---

## Milestone 0 — Course workflow

- [x] 0.1 Decide the rewrite scope with the lecturer's mini-project brief in
      mind: C++, 3D, surfaces, and the rendering pipeline implemented rather
      than called.
- [x] 0.2 Rewrite PRD.md, PLAN.md and TODO.md for the new project before
      writing code; keep the old documents' structure so the diff shows what
      changed.
- [x] 0.3 Commit per milestone, not once at the end.
      *Four substantive commits, each with a message that says what was
      verified.*

---

## Milestone 1 — Foundations (no rendering yet)

### Math and I/O
- [x] 1.1 `vecmath.h`: Vec2/3/4, row-major `Mat4` with a column-vector
      convention, `lookAt`, `perspective`, `orthographic`, Gauss–Jordan inverse,
      normal matrix.
- [x] 1.2 `png.{h,cpp}`: PNG writer with its own DEFLATE (LZ77 + fixed
      Huffman), Adler-32, CRC-32 and adaptive scanline filtering.
      *Verified: a 640×400 test image round-trips byte-identically through
      Pillow, at 7.17× compression. No zlib linked.*

### Mesh and topology
- [x] 1.3 `Mesh` with arbitrary-degree faces, Newell face normals,
      area-weighted vertex normals, bounds and bounding-sphere normalisation.
- [x] 1.4 `Topology`: edge interning, edge→face and vertex→edge/face adjacency,
      boundary flags, non-manifold detection.
- [x] 1.5 `orderedNeighbours()` and `orderedFacesAroundVertex()` — cyclic
      one-ring traversals with correct CW/CCW direction and boundary handling.
- [x] 1.6 `MeshStats`: V, E, F, Euler characteristic, boundary edges, face
      degree histogram, valence range, extraordinary count, edge lengths.
- [x] 1.7 Ten base cages: cube, tetrahedron, octahedron, icosahedron, torus,
      open plane, open cylinder, L-block, cross, pyramid — including a polycube
      builder that emits only non-shared faces.
      *Verified: χ = 2 for the closed solids, 0 for the torus and cylinder,
      1 for the open plane; no non-manifold edges anywhere.*

---

## Milestone 2 — The four surface schemes

- [x] 2.1 Catmull–Clark: face/edge/vertex points, `(Q + 2R + (n−3)S)/n`, quad
      construction per face corner.
      *Verified: cube level 1 = 26 V / 48 E / 24 F, exactly V+E+F of the cage.*
- [x] 2.2 Catmull–Clark boundary rules: (1,6,1)/8 along the border, valence-2
      corners pinned.
      *Verified: the corner of the open plane patch moves 0.00e+00 after three
      levels.*
- [x] 2.3 Loop, with Warren's β and boundary rules.
      *Verified: cube (triangulated) level 1 = 26 V / 48 tris; the open patch
      stays manifold.*
- [x] 2.4 Doo–Sabin: the α weights, F-faces, E-faces, V-faces.
      *Verified: cube level 1 = 24 V / 48 E / 26 F — the exact dual of
      Catmull–Clark — and every vertex has valence 4.*
- [x] 2.5 Doo–Sabin boundary handling via Chaikin points on the boundary
      polyline, including the corner case where both boundary edges belong to
      one face.
      *Verified: the open plane keeps χ = 1 through three levels.*
- [x] 2.6 Modified Butterfly: regular 8-point stencil, extraordinary stencils
      for K = 3, 4 and ≥ 5, averaging when both endpoints are irregular, and the
      Four-Point rule on boundaries.
      *Verified: after three levels on all ten cages, the maximum displacement
      of any original vertex is 0 — bit-exact interpolation.*
- [x] 2.7 `subdivide()` driver with automatic triangulation and a face budget.
      *Verified: nine requested levels on the torus stop at six, at 393 216
      faces, with the capped flag set.*
- [x] 2.8 Cross-check every scheme against the topology.
      *Verified: 4 schemes × 10 cages × 3 levels = 120 refinements, all
      preserving the Euler characteristic of their input and all manifold.*

---

## Milestone 3 — Renderer, application, documentation

### Rendering
- [x] 3.1 `RenderTarget` with supersampled colour + depth and a box-filter
      resolve.
- [x] 3.2 Triangle rasteriser: barycentric coverage, z-buffer, backface culling
      by signed area, perspective-correct interpolation.
- [x] 3.3 Sutherland–Hodgman clipping against the near plane, with attribute
      interpolation.
- [x] 3.4 Flat, Gouraud and Phong shading over Blinn–Phong with key/fill/ambient
      and a rim term.
- [x] 3.5 Depth-tested anti-aliased lines and points for wireframes, cages,
      control points, normals and extraordinary-vertex markers.
- [x] 3.6 `Viewport` remapping + scissor, so several independent views share one
      frame.
- [x] 3.7 Orbit camera with clamped pitch and zoom limits.

### 2D layer
- [x] 3.8 `Canvas`: blending, rectangles, signed-distance rounded rectangles,
      anti-aliased lines and circles, RGB export.
- [x] 3.9 `tools/genfont.py` bakes five faces into `src/font_data.h`; text
      renderer with UTF-8 decoding and per-glyph metrics.
      *Verified: specimen sheet rendered and inspected; a script cross-checks
      that every non-ASCII codepoint appearing in the source is present in the
      atlas.*

### Application
- [x] 3.10 `AppState` + `recompute()` + `renderFrame()`, with the window and the
      screenshot path sharing one code path.
- [x] 3.11 Surface mode with cage overlay, wireframe, extraordinary markers and
      normals.
- [x] 3.12 Curve mode: the three curve schemes on 3D control polygons.
- [x] 3.13 Terrain mode: diamond–square with roughness, seed and elevation ramp.
- [x] 3.14 Compare mode: four schemes on one cage, 2×2.
- [x] 3.15 Levels mode: levels 0–3 of one scheme, 2×2.
- [x] 3.16 HUD: scheme card, cage, level ticks, mesh statistics, Euler
      characteristic, longest edge in pixels, log-scaled growth chart, the active
      stencil, view toggles, key hints.
- [x] 3.17 Keyboard and mouse handling for every mode.

### Platform and tooling
- [x] 3.18 `x11window.*`: Xlib via `dlopen`, hand-declared structs, `XInitImage`
      + `XPutImage` presentation, `select()`-based idle.
      *Verified: the live window was captured with a throwaway `XGetImage` tool
      and compared against the offscreen render of the same state — identical
      colours, so the visual masks and byte order are right.*
- [x] 3.19 CLI: `--shot`, `--gallery`, `--selftest`, scene and view options,
      `--help`.
- [x] 3.20 `--selftest` with 15 checks covering topology, known counts,
      interpolation, boundary rules, curve schemes, budgets and the renderer.
- [x] 3.21 Makefile: `all`, `run`, `test`, `screenshots`, `font`, `clean`,
      `help`, with dependency generation.
      *Verified: `ldd subdivlab` lists only libc, libm, libstdc++ and libgcc.*

### Documentation
- [x] 3.22 14-image gallery generated by the application, with the recipes
      stored in `main.cpp`.
- [x] 3.23 README written around those images.
- [x] 3.24 REPORT.md with the concept mapping and the problems actually hit.

---

## Milestone 4 — Polish (optional, completed)

- [x] 4.1 HUD degrades gracefully on short windows instead of overflowing.
- [x] 4.2 Wireframe opacity scales with face count so dense meshes read as a
      grid rather than a dark smear.
- [x] 4.3 Meshes normalised by bounding-sphere radius so every cage frames
      identically.
- [x] 4.4 Curve parameters default per scheme, so switching to Four-Point does
      not land on an accidental fractal weight.
- [x] 4.5 Auto-spin, in-app PNG capture (`P`), and window-title status line.

---

## Deliberately not done

These were considered and rejected as out of scope for a mini-project; they are
listed so the boundary is explicit rather than accidental.

- Semi-sharp creases and per-edge sharpness weights.
- Adaptive / feature-driven subdivision.
- Exact limit-position and limit-normal evaluation (eigen-analysis of the
  subdivision matrix).
- Interactive 3D cage editing and OBJ import.
- Texture mapping, shadow mapping, ambient occlusion.
- A second window backend (Win32 / Cocoa / Wayland).
