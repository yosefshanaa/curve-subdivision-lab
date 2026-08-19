# Subdivision Lab 3D

Surface subdivision in a 3D renderer written from scratch in C++ — no OpenGL, no
GLFW, no image library, no font library. `g++ src/*.cpp -o subdivlab` is the
whole build.

Four schemes (**Catmull–Clark**, **Loop**, **Doo–Sabin**, **modified
Butterfly**) refine a control cage while a hand-written software rasteriser
draws the result: model/view/projection, near-plane clipping, z-buffer,
Blinn–Phong shading, and anti-aliased overlay lines for the wireframe and the
cage.

![Catmull-Clark on a cube](screenshots/01-catmull-clark-cube.png)

> Built as the Computer Graphics mini-project. It replaces an earlier
> single-file HTML demo that did *curve* subdivision in 2D; that version's
> schemes are still here (press `2`), but the project is now C++, three
> dimensional, and about **surfaces**.

## Build and run

```sh
make            # builds ./subdivlab
make run        # builds and opens the interactive window
make test       # runs the numeric self test
make screenshots  # regenerates every image in this README, from the app
```

Requirements: a C++17 compiler. That is the complete list.

The interactive window uses Xlib, but it is loaded at **runtime** with `dlopen`,
so there are no `-dev` packages to install and the program still builds and runs
on a machine with no display:

```sh
./subdivlab --shot out.png --mesh torus --scheme catmull-clark --level 3
```

`--shot` and the interactive window call the same `renderFrame()`, so a saved
PNG is pixel-for-pixel what the window shows.

## What it demonstrates

| Concept | Where you see it |
|---|---|
| **Control cage → limit surface** | The orange cage stays on screen over the refined surface at every level |
| **Approximating schemes** — Catmull–Clark, Loop, Doo–Sabin | The surface pulls *away* from the cage; the cage is only a hull |
| **Interpolating scheme** — modified Butterfly | The surface passes *exactly through* every cage vertex (verified bit-exact in `make test`) |
| **Primal vs dual refinement** | Catmull–Clark splits faces (26 V / 48 E / 24 F on a cube); Doo–Sabin builds the dual and gets exactly 24 / 48 / 26 |
| **Exponential growth — why ~5 levels is "infinity"** | Every level multiplies the face count by 4; the log-scaled chart and the Levels view make it explicit |
| **Extraordinary vertices** | Valence ≠ 4 (quad mesh) or ≠ 6 (triangle mesh) marked in pink — their *count never grows*, they just get isolated |
| **Boundary rules** | Open meshes: Catmull–Clark pins corners and runs a cubic B-spline along the border; Butterfly's boundary rule *is* the Four-Point curve scheme |
| **Topological invariants** | V − E + F is displayed live and checked for all 4 schemes × 10 cages in the self test |
| **Shading models** | Flat / Gouraud / Phong on the same mesh, one keypress apart |
| **Curve subdivision** (the 1D case these generalise) | Chaikin, Four-Point, midpoint displacement — in 3D space |
| **Procedural generation** | Diamond–square terrain: midpoint displacement with one more dimension |

## Screenshots

All of these are the program's own output, produced by `make screenshots`.

### Level progression — the face count ×4 each step

![Level progression](screenshots/02-level-progression.png)

### Four schemes on one cage

![Four schemes compared](screenshots/03-four-schemes-compared.png)

The point of this view: **Butterfly's surface touches every corner of the
cage** — it interpolates. The other three shrink inside it.

### Interpolating vs approximating, close up

| Modified Butterfly — passes through the cage | Loop — pulls away from it |
|---|---|
| ![Butterfly](screenshots/04-butterfly-interpolating.png) | ![Loop](screenshots/05-loop-icosahedron.png) |

### Doo–Sabin — the dual scheme

![Doo-Sabin](screenshots/06-doo-sabin-dual.png)

Every old face shrinks to a smaller copy, every edge and vertex becomes a new
face. On a closed mesh the result has **only valence-4 vertices**, which is why
the "extraordinary" counter reads 0.

### Extraordinary vertices stay put

![Extraordinary vertices](screenshots/07-extraordinary-vertices.png)

The Cross cage has 32 irregular vertices, and it still has exactly 32 at level
4 — while the vertex count itself climbs 32 → 122 → 482 → 1 922 → 7 682.
Subdivision does not create new extraordinary vertices; it isolates the ones the
cage already had, which is why the limit surface is smooth everywhere except at
finitely many points.

### Boundary rules on an open patch

![Boundary rules](screenshots/08-boundary-rules.png)

The four corners are pinned, the border converges to a cubic B-spline curve, and
V − E + F drops to 1 — the Euler characteristic of a disk rather than a sphere.

### Topology survives refinement

| Torus under Catmull–Clark — χ = 0 | Open cylinder under Loop — χ = 0, two boundary rims |
|---|---|
| ![Torus](screenshots/14-torus-genus-one.png) | ![Open cylinder](screenshots/15-open-cylinder-loop.png) |

Subdivision refines geometry, never topology. The torus keeps V − E + F = 0
through every level, the cylinder keeps its two boundary loops as smooth circles,
and the self test checks that invariant for all 4 schemes across all 10 cages.
The cylinder also shows Loop announcing that it triangulated the quad cage first,
because Loop is defined only on triangles.

### Shading models

| Flat — one normal per face | Phong — interpolated per pixel |
|---|---|
| ![Flat](screenshots/09-flat-shading.png) | ![Phong](screenshots/10-phong-shading.png) |

### Curve subdivision, the 1D ancestor

| Four-Point at w = 1/16 — smooth, interpolating | w = 0.25 — the same scheme goes fractal |
|---|---|
| ![Four-Point](screenshots/11-curve-four-point.png) | ![Fractal weights](screenshots/12-curve-fractal-weight.png) |

Chaikin on an open polyline, where the endpoints have to be anchored explicitly:

![Chaikin on an open helix cage](screenshots/16-curve-chaikin-open.png)

### Diamond–square terrain

![Terrain](screenshots/13-terrain-diamond-square.png)

Midpoint displacement one dimension up: each square gets a displaced centre, each
diamond a displaced edge midpoint, and the displacement range shrinks every
level. 129×129 vertices in about 1 ms.

## Controls

| Key | Action |
|---|---|
| `1` … `5` | Surface / Curve / Terrain / Compare / Levels |
| `S`, `⇧S` | Next / previous scheme |
| `M`, `⇧M` | Next / previous control cage (or curve preset) |
| `[` `]` | Subdivision level down / up |
| `,` `.` | Scheme parameter (curve weight, terrain roughness) |
| `F` | Flat → Gouraud → Phong |
| `W` `C` `G` | Wireframe / control cage / ground grid |
| `X` `N` | Extraordinary vertices / face normals |
| `A` | Auto-spin |
| `E` | Re-roll the random seed |
| `R` | Reset the camera |
| `H` | Hide the HUD |
| `P` | Save a 1440×880 PNG |
| `Q` / `Esc` | Quit |

Drag to orbit, scroll to zoom; arrow keys work too.

## Command line

```
subdivlab                        open the interactive window
subdivlab --shot FILE.png        render one frame and exit
subdivlab --gallery DIR          render every documentation screenshot
subdivlab --selftest             verify the schemes numerically

  --mode surface|curve|terrain|compare|levels
  --mesh cube|tetrahedron|octahedron|icosahedron|torus|plane|cylinder|
         lblock|cross|pyramid
  --scheme catmull-clark|loop|doo-sabin|butterfly|none
  --level N                      subdivision level
  --shading flat|gouraud|phong
  --width N --height N --ss N    canvas size and supersampling
  --yaw F --pitch F --dist F     camera
  --no-cage --no-wire --no-grid --no-hud --extraordinary --normals
```

Run `subdivlab --help` for the full list.

## How it is built

```
src/
  vecmath.h           Vec2/3/4, Mat4, lookAt / perspective
  mesh.{h,cpp}        polygon mesh + edge/vertex/face adjacency, boundary
                      detection, ordered one-ring traversal, mesh statistics,
                      ten base cages
  subdiv_surface.*    Catmull-Clark, Loop, Doo-Sabin, modified Butterfly
  subdiv_curve.*      Chaikin, Four-Point, midpoint displacement (in 3D)
  terrain.*           diamond-square heightfield
  render.{h,cpp}      the software pipeline: clipping, rasterisation, z-buffer,
                      shading, depth-tested lines and points, viewports
  canvas.{h,cpp}      2D surface, anti-aliased primitives, text rendering
  font_data.h         generated glyph atlases (tools/genfont.py)
  png.{h,cpp}         PNG writer including its own DEFLATE, CRC-32, Adler-32
  hud.cpp             the readout panel
  app.{h,cpp}         state, scene composition, input
  x11window.*         window via dlopen'd Xlib
  main.cpp            CLI, interactive loop, gallery, self test
```

About 5 000 lines, plus a 1 400-line generated font header.

A few decisions worth calling out:

- **The rasteriser is the point.** A course project that calls
  `glDrawElements` demonstrates the API; this one implements the pipeline the
  lectures describe — the matrices, the Sutherland–Hodgman near-plane clip, the
  barycentric coverage test, perspective-correct interpolation, and the depth
  buffer are all in `render.cpp`.
- **No dependencies, on purpose.** The PNG encoder writes its own DEFLATE
  stream, the text renderer draws from glyph atlases baked at build-authoring
  time by `tools/genfont.py` (the generated header is committed), and Xlib is
  `dlopen`ed. `ldd subdivlab` lists only libc, libm, libstdc++ and libgcc.
- **Anti-aliasing by supersampling.** The 3D scene renders at up to 3× and is
  box-filtered down; the HUD is drawn afterwards at native resolution so text
  stays sharp.
- **Subdivision is capped.** Face counts grow by 4× per level, so the driver
  stops before crossing a budget rather than freezing the machine.

## Verification

`make test` runs 15 checks in about a second:

```
Topology: Euler characteristic V-E+F is preserved by every scheme
  [PASS] 40 mesh/scheme combinations keep their Euler characteristic
Known refinement counts on the cube
  [PASS] Catmull-Clark level 1: 26 V / 48 E / 24 F  (V+E+F of the cage)
  [PASS] Doo-Sabin level 1: 24 V / 48 E / 26 F  (exactly the dual)
  [PASS] Doo-Sabin makes every vertex valence 4
Interpolating vs approximating
  [PASS] Butterfly leaves every control vertex exactly in place
  [PASS] Catmull-Clark pulls control vertices inward (approximating)
Boundary rules
  [PASS] Catmull-Clark pins the corners of an open patch
  [PASS] Loop keeps an open patch manifold
Curve schemes
  [PASS] Chaikin on a square yields 8 points
  [PASS] Chaikin cuts at 1/4 and 3/4 (the classic 1:3 cut)
  [PASS] Four-Point with w = 0 gives plain midpoints
  [PASS] Four-Point keeps the control points (interpolating)
Budgets
  [PASS] the face budget stops runaway subdivision
Renderer
  [PASS] renderFrame produces a non-trivial image
  [PASS] canvas converts to a packed RGB buffer
```

Measured refinement cost on the 96-face torus cage (`-O2`, one core):

| Level | Catmull–Clark | Doo–Sabin | Loop | Butterfly |
|---|---|---|---|---|
| 2 | 0.5 ms | 0.6 ms | 0.7 ms | 1.1 ms |
| 3 | 1.8 ms | 2.3 ms | 3.3 ms | 4.6 ms |
| 4 | 8.2 ms | 10.1 ms | 13.6 ms | 26.4 ms |
| 5 | 39 ms | 45 ms | 75 ms | 122 ms |

## Project documents

- [PRD.md](PRD.md) — requirements, success criteria, non-goals
- [PLAN.md](PLAN.md) — architecture and the exact scheme mathematics
- [TODO.md](TODO.md) — milestone task list with verification results
- [REPORT.md](REPORT.md) — course report: what was built, what broke, what each
  feature demonstrates
