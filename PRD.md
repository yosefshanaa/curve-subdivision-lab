# PRD — Subdivision Lab 3D

## 1. Overview

**Product:** A C++ desktop application that demonstrates **surface subdivision**
in 3D, rendered by a software rasteriser written for this project.

**Form factor:** A single native binary. `make` is the whole build; the only
requirement is a C++17 compiler. There is no OpenGL, no windowing toolkit, no
image library and no font library to install — the window is obtained by
`dlopen`ing Xlib at runtime, and the program also runs with no display at all.

**History:** v1 of this project was a single-file HTML/Canvas demo of *curve*
subdivision in 2D. This document specifies the rewrite: same subject, but in
C++, in three dimensions, and centred on surfaces. The curve schemes survive as
one mode of the new app because they are the one-dimensional ancestors of the
surface rules.

## 2. Goals

1. Make the **control cage → limit surface** relationship visible and
   manipulable in 3D.
2. Show the difference between **approximating** and **interpolating** surface
   schemes as directly as the 2D version showed it for curves.
3. Make the **cost** of subdivision concrete: the face count multiplies by four
   per level, which is why "about five levels" is effectively the limit surface.
4. Show the parts of the theory that only exist for surfaces: **extraordinary
   vertices**, **boundary rules**, **primal vs dual refinement**, and the
   **Euler characteristic** as an invariant that a correct implementation must
   preserve.
5. Implement the **rendering pipeline itself**, not a wrapper around one, so the
   project demonstrates the transformation/clipping/rasterisation material as
   well as the subdivision material.

## 3. Target users

- **The lecturer**, during a demo or while grading: must reach every concept
  within about two minutes of keyboard use, and must be able to build the
  project without installing anything.
- **The student (author)**, presenting live: controls that are easy to drive and
  hard to break.
- Anyone reading the repository: the README must convey the result without
  running anything, using images the program itself produced.

## 4. Success criteria

- **S1** — `make && make run` works on a stock Linux install with only a
  compiler present. No `-dev` packages, no vendored third-party source.
- **S2** — A first-time viewer sees a cage refine into a smooth surface within
  10 seconds of the window opening.
- **S3** — The interpolating/approximating distinction is visually obvious in a
  single frame (the Compare view).
- **S4** — The exponential growth of subdivision is on screen as a number *and*
  as a picture.
- **S5** — Correctness is demonstrated, not asserted: a self test verifies the
  schemes against known counts and invariants and prints PASS/FAIL.
- **S6** — Every screenshot in the documentation is reproducible with one
  command, from the application, with the recipe recorded in the source.
- **S7** — The app never freezes: subdivision is capped by a face budget.
- **S8** — It runs headless, so it can render documentation on a machine with no
  display.

## 5. Functional requirements

### Subdivision schemes

- **FR-1** — Implement **Catmull–Clark** for arbitrary polygon meshes: face
  points, edge points, and the vertex rule `(Q + 2R + (n−3)S) / n`.
- **FR-2** — Implement **Loop** for triangle meshes, with Warren's β. Non-triangle
  cages are triangulated first and the UI says so.
- **FR-3** — Implement **Doo–Sabin**, including the F/E/V face construction, so
  the dual character of the scheme is visible.
- **FR-4** — Implement **modified Butterfly** (Zorin): interpolating, with the
  regular 8-point stencil, the extraordinary-vertex stencil for valence 3, 4 and
  ≥ 5, and averaging when both endpoints are extraordinary.
- **FR-5** — Boundary rules for every scheme that can meet a boundary:
  Catmull–Clark pins corners and uses the (1, 6, 1)/8 cubic B-spline rule along
  the border; Loop uses (1, 6, 1)/8; Doo–Sabin closes against Chaikin points;
  Butterfly falls back to the Four-Point curve rule.
- **FR-6** — A level control, 0 to 5, with a face budget that stops refinement
  before the machine does.
- **FR-7** — At least eight control cages, including closed solids, a torus (χ = 0),
  an open patch and an open cylinder (two boundary loops), and non-convex
  polycubes.

### The 1D case and procedural generation

- **FR-8** — Curve mode: Chaikin, Four-Point and random midpoint displacement,
  operating on 3D control polygons, with the "wrong weights go fractal"
  behaviour reachable from the parameter control.
- **FR-9** — Terrain mode: diamond–square, seeded, with a roughness control and
  an elevation colour ramp — midpoint displacement one dimension up.

### Rendering

- **FR-10** — A software 3D pipeline: model/view/projection matrices,
  near-plane clipping, perspective divide, viewport transform, backface culling,
  z-buffer, and perspective-correct attribute interpolation.
- **FR-11** — Three shading models — flat, Gouraud and Phong — switchable at
  runtime on the same mesh.
- **FR-12** — Overlays that respect the depth buffer: the refined wireframe, the
  control cage, control points, face normals, and extraordinary-vertex markers.
- **FR-13** — Anti-aliasing by supersampling, resolved before the HUD is drawn
  so text stays sharp.
- **FR-14** — Multiple viewports in one frame, for the four-scheme comparison
  and the level progression.

### Readout

- **FR-15** — A live panel showing the active scheme and its classification, the
  cage, the level, V / E / F, **V − E + F**, the extraordinary-vertex count, the
  longest mesh edge in pixels, the refinement time, and a log-scaled chart of
  faces per level.
- **FR-16** — The active scheme's stencil written out, so the picture and the
  formula are on screen together.
- **FR-17** — The HUD degrades gracefully on small windows rather than
  overflowing.

### Interaction and tooling

- **FR-18** — Orbit/zoom by mouse or keyboard; every mode, scheme, cage and
  overlay reachable by a single keypress.
- **FR-19** — `--shot` renders one frame offscreen to a PNG using the same code
  path as the window.
- **FR-20** — `--gallery` renders the complete documentation screenshot set.
- **FR-21** — `--selftest` verifies the schemes numerically and exits non-zero
  on failure.
- **FR-22** — PNG output without linking an image library.

## 6. Non-goals

- **No GPU rendering.** Implementing the pipeline is the point.
- **No mesh editing.** Cages are chosen from a built-in set; there is no vertex
  dragging in 3D and no file import.
- **No creases, no adaptive subdivision, no limit-position evaluation.** These
  are real parts of the literature but well beyond a mini-project.
- **No texture mapping, no shadows, no global illumination.**
- **No cross-platform window backends.** The renderer is portable C++; only the
  window is X11-specific, and its absence degrades to headless rendering rather
  than a build failure.
- **No third-party code**, vendored or linked, beyond the C++ standard library.

## 7. Layout

```
+------------------+--------------------------------------------------+
| Subdivision Lab  |                                                  |
| mode tabs        |                                                  |
|                  |                                                  |
| SCHEME           |                  3D VIEWPORT                     |
|  Catmull-Clark   |                                                  |
|  approximating   |     - shaded limit surface                       |
|                  |     - refined wireframe                          |
| CONTROL CAGE     |     - control cage + control points (orange)     |
| LEVEL  ####--    |     - extraordinary vertices (pink)              |
|                  |                                                  |
| REFINED MESH     |                                                  |
|  V / E / F       |                                                  |
|  V - E + F       |                                                  |
|  extraordinary   |                                                  |
|  longest edge px |                                                  |
|                  |                                                  |
| FACES PER LEVEL  |                                                  |
|  [log bar chart] |                                                  |
|                  |                                                  |
| REFINEMENT RULE  |                                                  |
| VIEW toggles     |                                                  |
+------------------+--------------------------------------------------+
|                  | key hints                                        |
+------------------+--------------------------------------------------+
```

In Compare and Levels modes the viewport is split into a 2×2 grid of
independent views sharing one camera.

## 8. Assumptions

- **A1** — Desktop Linux; WSLg counts. The binary is portable C++ apart from the
  X11 window.
- **A2** — Five levels is a sufficient maximum: a cube reaches 6 144 faces, and
  the surface is visually converged well before that.
- **A3** — The lecture's subdivision material (control polygon/cage, limit
  curve/surface, approximating vs interpolating, weights and fractal behaviour,
  "≈5 iterations is infinity") is the scope. Surface subdivision extends it in
  the direction the lecture itself named as the 3D generalisation.
- **A4** — English UI text, no localisation.

## 9. Course submission requirements

- **D1 — Git history:** incremental commits, one per meaningful milestone. A
  single final "everything" commit is treated as suspicious.
- **D2 — Report:** `REPORT.md` with real content — what was built, screenshots
  of each behaviour, the mapping from features to lecture concepts, and the
  problems actually encountered and how they were diagnosed.
- **D3 — Reproducibility:** every figure in the documentation regenerable with
  one command, and every correctness claim backed by the self test.
