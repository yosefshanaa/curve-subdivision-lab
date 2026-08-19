# REPORT — Subdivision Lab 3D

Computer Graphics mini-project. Author: yosefshanaa.

---

## 1. What was built

A C++ application that refines a control cage with four surface subdivision
schemes and draws the result with a 3D renderer written for this project — no
OpenGL, no windowing toolkit, no image or font library. `make` is the entire
build and the only requirement is a C++17 compiler.

| | |
|---|---|
| **Subject** | Surface subdivision: Catmull–Clark, Loop, Doo–Sabin, modified Butterfly |
| **Also covered** | Curve subdivision (Chaikin, Four-Point, midpoint displacement) and diamond–square terrain |
| **Rendering** | Own software rasteriser: MVP transforms, near-plane clipping, z-buffer, backface culling, flat/Gouraud/Phong |
| **Size** | ~5 000 lines of C++ across 21 files, plus a 1 400-line generated font header |
| **Dependencies** | None. `ldd` shows libc, libm, libstdc++, libgcc |
| **Verification** | `make test` — 16 checks, including 120 refinements audited for both topology and orientation |

![Catmull-Clark on a cube](screenshots/01-catmull-clark-cube.png)

### Relationship to v1

The first version of this project was a single-file HTML/Canvas app that did
**curve** subdivision in **2D**. This is a rewrite rather than a port: the
language, the dimensionality and the subject all changed. The curve schemes are
still present — as one mode out of five — because they are the one-dimensional
ancestors of the surface rules, and putting them side by side is the clearest
way to show that relationship.

The correspondence turned out to be tighter than expected, and it is now one of
the things the app teaches:

| Curve scheme | Surface counterpart | Why they are the same idea |
|---|---|---|
| Chaikin corner cutting | Doo–Sabin | Both approximating; Doo–Sabin's boundary rule *is* Chaikin, so an open Doo–Sabin surface has a Chaikin curve for a border |
| Four-Point | Modified Butterfly | Both interpolating; Butterfly's boundary rule *is* the Four-Point rule, coefficients and all (9/16, −1/16) |
| Midpoint displacement | Diamond–square | The same recipe with one more dimension: displace a midpoint, halve the range, repeat |

---

## 2. Feature → concept mapping

### Control cage and limit surface

![Level progression](screenshots/02-level-progression.png)

The Levels view puts levels 0–3 of one scheme in a single frame with the cage
drawn over each. The face counts under the tiles — 6, 24, 96, 384 — are the
lecture's exponential growth made literal, and the log-scaled chart in the panel
exists because a linear chart of those numbers is one bar and three slivers.

This is also where "about five levels is infinity" becomes concrete. Level 3 and
level 4 are already hard to tell apart on screen; the HUD's *longest edge in
pixels* readout is the honest version of that observation, and it falls below
2 px — flagged as "≈ limit surface" — once the edges are smaller than the
rasteriser can resolve.

### Approximating vs interpolating

![Four schemes compared](screenshots/03-four-schemes-compared.png)

One cage, four schemes, one camera. Butterfly's surface visibly touches every
corner of the cube; the other three pull away from it. The self test makes the
claim exact: after three levels on all ten cages, the maximum displacement of
any original vertex under Butterfly is **0** — bit-exact, not
approximately-zero (still 0 at level 4). Catmull–Clark moves a cube corner by
0.44 on the same normalised cage.

This is the surface version of the distinction the 2D app made with Chaikin vs
Four-Point, and it is the single most useful frame in the project.

### Primal vs dual refinement

![Doo-Sabin](screenshots/06-doo-sabin-dual.png)

Catmull–Clark refines a cube into 26 V / 48 E / 24 F. Doo–Sabin refines the same
cube into 24 V / 48 E / 26 F — the vertex and face counts *swapped*, with the
edge count fixed. That is the definition of the dual, and it drops out of the
implementation rather than being arranged. Doo–Sabin also drives every vertex to
valence 4 on a closed mesh, which is why the extraordinary counter reads 0 in
the screenshot above.

### Extraordinary vertices

![Extraordinary vertices](screenshots/07-extraordinary-vertices.png)

The Cross cage has 32 vertices of irregular valence. After four levels the mesh
has 7 682 vertices — and still exactly 32 extraordinary ones:

| Level | 0 | 1 | 2 | 3 | 4 |
|---|---|---|---|---|---|
| Vertices | 32 | 122 | 482 | 1 922 | 7 682 |
| Extraordinary | 32 | 32 | 32 | 32 | 32 |

Subdivision does not create irregularity; it isolates the irregularity the cage
already had, surrounding each irregular vertex with an ever-larger regular
region. That is exactly why the limit surface is smooth everywhere except at
finitely many points, and why those points are where the analysis in the
literature gets hard.

### Boundary rules and topological invariants

![Boundary rules](screenshots/08-boundary-rules.png)

The open patch keeps V − E + F = **1** at every level: the Euler characteristic
of a disk, not of a sphere. The four corners stay pinned, and the border
converges to a cubic B-spline through the boundary polygon.

Euler characteristic turned out to be the single most useful debugging tool in
the project. It is cheap, it is invariant under every one of these schemes, and
when an implementation is wrong it does not drift — it collapses. See §4.1.

### Rendering pipeline

| Flat — one normal per face | Phong — interpolated per pixel |
|---|---|
| ![Flat](screenshots/09-flat-shading.png) | ![Phong](screenshots/10-phong-shading.png) |

Same mesh, same level, one keypress apart. Flat shading also makes the
subdivision level visible in a second way: you can count the facets.

Everything behind these images is in `render.cpp` — `Mat4::lookAt` and
`Mat4::perspective`, Sutherland–Hodgman clipping against the near plane, the
perspective divide, the viewport transform, backface culling by the sign of the
screen-space signed area, barycentric coverage with a z-buffer, and
perspective-correct interpolation of position and normal. Choosing to write this
instead of calling OpenGL meant the transformation and rasterisation lectures
became part of the project rather than a prerequisite for it.

### Curves and procedural generation

| Four-Point at w = 1/16 | the same scheme at w = 0.25 |
|---|---|
| ![Four-Point](screenshots/11-curve-four-point.png) | ![Fractal](screenshots/12-curve-fractal-weight.png) |

The weight matters. At w = 1/16 the limit curve is smooth and passes through
every control point; push w past 1/8 and the same scheme produces something
nowhere-differentiable. The HUD detects the condition from the state, so the
warning appears whether the preset or the slider got it there.

![Terrain](screenshots/13-terrain-diamond-square.png)

Diamond–square: 129 × 129 vertices in about 1 ms, seeded so the terrain is
stable across redraws, with roughness controlling how much energy stays in the
fine levels.

---

## 3. Correctness

`make test` runs 16 checks in about a second:

- **Topology.** 4 schemes × 10 cages × 3 levels = 120 refinements. Every one
  preserves the Euler characteristic of its input and produces no non-manifold
  edge.
- **Orientation.** The same refinements audited by directed edge: every directed
  edge appears exactly once, and every interior edge appears once in each
  direction. This is the check that catches backwards-wound faces, which leave
  V, E, F — and therefore χ — untouched. See §4.1.
- **Known counts.** Catmull–Clark on the cube gives 26/48/24; Doo–Sabin gives
  24/48/26 with all valences equal to 4.
- **Interpolation.** Butterfly's displacement of original vertices is exactly
  zero; Catmull–Clark's is not.
- **Boundaries.** The plane's corners are pinned under Catmull–Clark; Loop keeps
  the open patch manifold.
- **Curve schemes.** Chaikin on a unit square gives the eight known points at
  1/4 and 3/4; Four-Point at w = 0 gives midpoints and at w = 1/16 keeps the
  control points.
- **Budget.** Nine requested levels on the torus stop at six.
- **Renderer.** `renderFrame` produces a non-trivial image of the requested
  size.

Measured refinement cost on the 96-face torus cage (`-O2`, single core):

| Level | faces | Catmull–Clark | Doo–Sabin | Loop | Butterfly |
|---|---|---|---|---|---|
| 2 | 1 536 | 0.5 ms | 0.6 ms | 0.7 ms | 1.1 ms |
| 3 | 6 144 | 1.8 ms | 2.3 ms | 3.3 ms | 4.6 ms |
| 4 | 24 576 | 8.2 ms | 10.1 ms | 13.6 ms | 26.4 ms |
| 5 | 98 304 | 39 ms | 45 ms | 75 ms | 122 ms |

Butterfly is the slowest because its stencil needs an ordered one-ring walk per
edge; the others only need averages over unordered adjacency.

---

## 4. Problems encountered

### 4.1 Doo–Sabin looked correct at level 1 and disintegrated at level 2

The first Doo–Sabin implementation produced a perfect dual of the cube:
24 V / 48 E / 26 F, χ = 2. Level 2 gave χ = −46. Level 3 gave χ = −322.

χ collapsing that way means edges are not being shared — faces that should meet
along an edge are each contributing their own.

The level-1 mesh was the place to look, but its Euler characteristic was clean,
because winding does not change V, E or F. What was wrong was **orientation**, and
that needs a different audit: count every directed edge `(a, b)`; in a
consistently oriented closed manifold each appears exactly once, and so does its
reverse. Running that over the level-1 output found **24 violations** — which is
also why level 2 fell apart rather than merely looking odd, since the CW/CCW ring
walks Doo–Sabin depends on were then traversing a mesh whose windings
disagreed.

Two separate bugs:

1. **V-faces were reversed.** I had assumed the CCW face fan around a vertex
   produced a clockwise polygon and flipped it. It does not; the fan order is
   already correct.
2. **E-face winding depended on storage order.** `Topology::Edge` stores its two
   incident faces in whatever order they were first encountered. The quad joining
   them must be built from *the face that traverses `a → b`* and *the face that
   traverses `b → a`*, not from `f0` and `f1` — otherwise the quad comes out
   backwards whenever the arbitrary storage order disagrees with the traversal
   direction, which on the cube was 5 of the 12 edges.

After both fixes: χ = 2 at every level, every vertex valence 4, zero
directed-edge violations. The audit that found the bug is now
`orientationDefects()` in `mesh.cpp` and runs over all 40 mesh/scheme
combinations in the self test. Two lessons generalise. *"It renders correctly" is not
evidence that a mesh operator is correct* — the broken level-1 mesh was
indistinguishable from the fixed one on screen. And *no single invariant is
enough*: the Euler characteristic caught the collapse but pointed at the wrong
level, and only the orientation audit located the actual defect.

### 4.2 Doo–Sabin corners on an open patch

With the closed case fixed, the open plane still failed. The bug was in
identifying the two ends of an open face fan: I picked them by asking which
boundary edge belonged to the first face in the fan. At a **corner** of the
patch, the vertex has one incident face and *both* of its boundary edges belong
to it, so that test is a coin flip.

The fix is to use direction rather than membership: the fan runs CCW, so it opens
on the edge *leaving* `v` in its first face (`v → next`) and closes on the edge
*entering* `v` in its last face (`prev → v`). Corners work because those are two
different edges even when they share a face.

### 4.3 Four-Point defaulted to an accidental fractal

Switching curve schemes kept the previous scheme's parameter. Chaikin's default
cut ratio is 1/4; Four-Point's default weight is 1/16. Pressing `S` therefore
handed Four-Point w = 0.25 — twice the value at which it stops converging to
anything smooth — and the first Four-Point screenshot came out as a jagged mess
with the app correctly (and unhelpfully) flagging "w far from 1/16 → fractal".

Each scheme now carries its own default and the parameter is reset on a scheme
change unless the caller asked for a value. The fractal case did not disappear —
it earned a screenshot of its own, which is more useful than an accident.

### 4.4 Rendering choices that turned out to matter

- **Bounding box vs bounding sphere.** Normalising cages so their bounding *box*
  fits a fixed extent framed a cube's corners at √3 while an icosahedron sat
  well inside the frame. Normalising by bounding-*sphere* radius makes every
  cage frame identically, and the clipped-off cage corners in the early
  screenshots went away.
- **Wireframes darken dense meshes.** A fixed-opacity wireframe over 1 536 faces
  stops reading as a grid and just makes the surface muddy. Opacity now scales
  with face count.
- **Supersample before the HUD, not after.** The scene renders at up to 3× and
  is box-filtered down; the HUD is drawn afterwards at native resolution. Drawing
  text into the supersampled buffer would have softened every glyph.

### 4.5 Text and images without libraries

Refusing dependencies meant two things had to be built:

- **PNG.** `png.cpp` implements DEFLATE (LZ77 with hash chains, fixed Huffman),
  Adler-32, CRC-32 and per-scanline adaptive filtering. It was validated by
  writing a test image, reading it back with Pillow, and comparing the raw pixel
  buffers — byte-identical, at 7.17× compression.
- **Text.** `tools/genfont.py` bakes Inter and DejaVu Sans Mono into 8-bit alpha
  glyph atlases with per-glyph metrics. The generated header is committed, so
  the build never needs Python. The failure mode here is quiet: any character
  not in the atlas renders as `?`, which is how `V − E + F` shipped for a while
  as `V ? E + F` (U+2212 MINUS SIGN was not baked). There is now a script that
  extracts every non-ASCII codepoint from the source and checks it against the
  atlas.

### 4.6 A window with no X11 headers

The machine had `libX11.so.6` but no development headers, and no way to install
them. Rather than drop the interactive window, `x11window.cpp` declares the Xlib
entry points and the few public structs it needs and resolves them with `dlopen`
/ `dlsym`. Pixels reach the screen by filling an `XImage` by hand and calling
`XInitImage` + `XPutImage`, which avoids `XCreateImage`/`XDestroyImage` and keeps
ownership of the buffer.

This is the one place in the project that relies on an assumption rather than a
guarantee: the layout of those structs and the 24-bit TrueColor visual. It was
verified rather than trusted — a throwaway tool captured the live window with
`XGetImage` and the result was compared against the offscreen render of the same
state. Identical, so the masks and byte order are right. The side benefit is that
the build has no X dependency at all, and the program still runs headless.

---

## 5. What I would do next

- **Semi-sharp creases** (Pixar's edge-sharpness weights). Currently every edge
  is smooth; creases are what make subdivision usable for real modelling.
- **Exact limit positions.** Pushing vertices to their limit positions via the
  eigenstructure of the subdivision matrix would let the surface be drawn
  correctly at a lower level.
- **Adaptive subdivision.** Refine only where the surface is curved or near the
  silhouette, instead of multiplying every face by four.
- **Threading the rasteriser.** Row-banded parallelism would make level 5
  interactive at full supersampling; the pipeline is already free of shared
  mutable state per triangle.

---

## 6. Reproducing everything in this report

```sh
make            # build
make test       # the 15 correctness checks quoted in §3
make screenshots  # regenerate all 14 images used here
make run        # the interactive app
```

Every figure above came out of the application itself; the command line for each
one is recorded in the gallery table in `src/main.cpp`.
