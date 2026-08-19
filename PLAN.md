# PLAN — Subdivision Lab 3D

Technical plan for the application specified in [PRD.md](PRD.md). Single
developer, no dependencies beyond the C++ standard library.

## 1. Architecture

```
main.cpp        CLI, interactive loop, gallery, self test
   |
app.{h,cpp}     AppState -> recompute() -> renderFrame()
   |            (one state object; both the window and --shot go through here)
   +-- hud.cpp             the readout panel, drawn on the resolved canvas
   +-- render.{h,cpp}      Camera, RenderTarget, Renderer (the 3D pipeline)
   |      +-- canvas.{h,cpp}  Canvas, 2D primitives, text
   |             +-- font_data.h   generated glyph atlases
   +-- subdiv_surface.*    Catmull-Clark / Loop / Doo-Sabin / Butterfly
   +-- subdiv_curve.*      Chaikin / Four-Point / midpoint displacement
   +-- terrain.*           diamond-square
   |      +-- mesh.{h,cpp}    Mesh, Topology, MeshStats, base cages
   |             +-- vecmath.h
   +-- png.{h,cpp}         PNG + DEFLATE writer
   +-- x11window.*         dlopen'd Xlib window (optional at runtime)
```

**Data flow is one-directional.** Input mutates `AppState`, `recompute()`
rebuilds the derived meshes, `renderFrame()` draws. Nothing else touches the
framebuffer. Any visual bug is therefore either in a `*Step()` function (dump
the mesh and check `V − E + F`) or in `render.cpp`.

**Every subdivision scheme is a pure function `Mesh -> Mesh`.** They share no
state, cache nothing, and are individually testable. Refinement is cheap enough
(tens of milliseconds at level 5) that the app recomputes from the cage on every
change instead of maintaining an incremental structure.

## 2. Data model

```cpp
struct Mesh {
    std::vector<Vec3> V;                  // vertex positions
    std::vector<std::vector<int>> F;      // faces: CCW loops of vertex indices
    std::vector<Vec3> vertexColor;        // optional (terrain elevation ramp)
    std::vector<Vec3> faceNormal, vertexNormal;   // derived
};
```

Faces are arbitrary index loops rather than fixed-size quads or triangles,
because the four schemes disagree about face degree: Catmull–Clark turns
anything into quads, Loop and Butterfly require triangles, and Doo–Sabin emits
an n-gon around every old vertex.

```cpp
struct Topology {
    struct Edge { int a, b, f0, f1; };    // a < b; f1 == -1 on a boundary
    std::vector<Edge> edges;
    std::vector<std::vector<int>> vertEdges, vertFaces, faceEdges;
    std::vector<char> vertBoundary;
};
```

`buildTopology()` is rebuilt from scratch on every step. It is O(E log E)
because edges are interned through a `std::map`, which is irrelevant next to the
cost of the geometry at these sizes.

Two traversals do the real work for the harder schemes:

- `orderedNeighbours(v)` — the one-ring in cyclic order, starting from a
  boundary edge when there is one. Butterfly's extraordinary stencil needs the
  neighbours *in order*, indexed from the edge being split.
- `orderedFacesAroundVertex(v)` — the face fan in **counter-clockwise** order,
  plus whether the ring closes. Crossing the edge `(prev_f(v), v)` moves CCW
  around `v`; crossing `(v, next_f(v))` moves CW. Doo–Sabin's vertex faces are
  built directly from this.

## 3. Surface scheme mathematics

Let the cage have vertex positions `P`, faces `F`, edges `E`.

### 3.1 Catmull–Clark (approximating, quads from anything)

```
face point    F_f = centroid of face f
edge point    E_e = (P_a + P_b + F_left + F_right) / 4        interior
              E_e = (P_a + P_b) / 2                           boundary
vertex point  V_v = (Q + 2R + (n-3)S) / n                     interior
                Q = mean of the adjacent face points
                R = mean of the adjacent edge midpoints
                S = P_v,  n = valence
              V_v = (P_prev + 6 P_v + P_next) / 8             boundary
              V_v = P_v                                       corner (valence 2)
```

New faces: for each corner `i` of each face `f`, the quad
`(V_{v_i}, E_{next edge}, F_f, E_{prev edge})`. Face count becomes `Σ deg(f)`.

The boundary rule is the cubic B-spline curve rule, so the border of an open
patch converges to a cubic B-spline through the boundary polygon, and pinning
valence-2 vertices keeps the corners of a rectangular patch in place.

### 3.2 Loop (approximating, triangles)

```
odd  (new edge vertex)   3/8 (a + b) + 1/8 (c + d)            interior
                         1/2 (a + b)                          boundary
even (old vertex)        (1 - n*beta) v + beta * sum(neighbours)
                         beta = (5/8 - (3/8 + 1/4 cos(2pi/n))^2) / n
                         3/4 v + 1/8 (prev + next)            boundary
```

`beta` evaluates to exactly 3/16 at n = 3, matching Warren's special case, so no
branch is needed. Each triangle becomes four.

### 3.3 Doo–Sabin (approximating, dual, n-gons)

For a face with `n` corners, corner `i` produces

```
P'_i = sum_j alpha_ij P_j
alpha_ii = (n + 5) / 4n
alpha_ij = (3 + 2 cos(2pi (i-j) / n)) / 4n,  i != j
```

The weights sum to 1: `sum_{j!=i} alpha_ij = (3(n-1) - 2) / 4n = (3n-5)/4n`,
and `(3n-5)/4n + (n+5)/4n = 1`.

Three families of new faces:

1. **F-faces** — one shrunken copy of each old face.
2. **E-faces** — one quad per old edge, spanning the gap between the two
   shrunken faces.
3. **V-faces** — one polygon per old vertex, joining the new points contributed
   by every face in its fan.

The winding of the E-faces is the subtle part. `Topology::Edge` stores its two
faces in whatever order they were encountered, so the quad must be built from
the face that traverses `a → b` and the face that traverses `b → a`, not from
`f0` and `f1`. Building it from `f0`/`f1` produces a mesh that *looks* right at
level 1 and disintegrates at level 2 (see [REPORT.md](REPORT.md) §4.1).

On a boundary, the gap is closed against the Chaikin points `(3a+b)/4` and
`(a+3b)/4` of the boundary polyline, so the border converges to the quadratic
B-spline — which is Chaikin corner cutting. Doo–Sabin *is* the surface
generalisation of Chaikin, and its boundary behaviour says so.

### 3.4 Modified Butterfly (interpolating, triangles)

Old vertices are copied unchanged; only the new edge vertices need a rule.

**Regular case** (both endpoints interior with valence 6):

```
Q = 1/2 (a + b) + 1/8 (c + d) - 1/16 (e1 + e2 + e3 + e4)
```

where `c, d` are the vertices opposite the edge in its two triangles and
`e1..e4` are the four "wing" vertices beyond them.

**Extraordinary case** — split of edge `v–w` where `v` has valence `K != 6`:

```
Q = 3/4 P_v + sum_{j=0}^{K-1} s_j P_{ring_j},   ring_0 = w
K = 3:  s = ( 5/12, -1/12, -1/12 )
K = 4:  s = (  3/8,     0,  -1/8, 0 )
K >= 5: s_j = ( 1/4 + cos(2pi j/K) + 1/2 cos(4pi j/K) ) / K
```

`sum_j s_j = 1/4` for every K, so the stencil is affine. When both endpoints are
extraordinary, average the two estimates.

**Boundary** — the Four-Point curve scheme:
`Q = 9/16 (a + b) − 1/16 (a_prev + b_next)`. This is the cleanest link in the
whole project: the interpolating surface scheme degenerates on its boundary into
exactly the interpolating curve scheme from the 2D version of this app.

The one-ring ordering is direction-independent here, because `s_j` depends on
`cos(2pi j/K)` and reversing the ring maps `j -> K-j`, which leaves the cosines
unchanged.

### 3.5 Driver

```cpp
SubdivResult subdivide(cage, scheme, levels, faceBudget = 400000);
```

Triangulates first when the scheme needs it, then applies the step function
`levels` times, stopping early if the *next* step would exceed the budget (each
step multiplies faces by about 4). Records face/vertex counts per level for the
growth chart, and the elapsed time.

## 4. Curve schemes

Kept from the 2D version, lifted to `Vec3`. A shared index resolver wraps for
closed polygons and reflects a phantom point (`P₋₁ = 2P₀ − P₁`) for open ones, so
one formula covers every edge.

| Scheme | Rule | Surface counterpart |
|---|---|---|
| Chaikin | each edge → `(1−t)A + tB`, `tA + (1−t)B`; `t = 1/4` | Doo–Sabin |
| Four-Point | keep `P`, insert `(½+w)(Pᵢ+Pᵢ₊₁) − w(Pᵢ₋₁+Pᵢ₊₂)`; `w = 1/16` | Butterfly |
| Midpoint displacement | keep `P`, insert the midpoint displaced by ±`r·|edge|/2`, range halving per level | diamond–square |

## 5. Terrain

Diamond–square on a `(2^levels + 1)²` grid. The displacement range is multiplied
by `decay = 0.42 + 0.22·roughness` each level; `decay = 0.5` tracks the halving
grid spacing exactly, so values below it read as smooth and above it as rough.
Heights are normalised to [−1, 1], values below sea level are flattened into
water, and vertex colours come from an elevation ramp.

## 6. Rendering pipeline

```
world position
  -> vp = projection * view          (Mat4::perspective, Mat4::lookAt)
  -> clip space
  -> Sutherland-Hodgman against w > epsilon      (near plane only)
  -> perspective divide -> NDC
  -> viewport transform -> supersampled pixels
  -> backface cull by the sign of the screen-space signed area
  -> barycentric coverage + z-buffer
  -> shade
```

Only the near plane is clipped; the other five planes are handled for free by
clamping the raster bounding box to the scissor rectangle. Depth is the
post-divide NDC z, which is linear in screen space, so plain barycentric
interpolation of it is correct. Colour and normals use the perspective-correct
form (interpolate attribute/w and 1/w, then divide).

**Shading.** Flat evaluates the lighting once per triangle at the centroid,
Gouraud at the three corners with interpolation of the *result*, Phong
interpolates the normal and evaluates per pixel. Blinn–Phong with a key light, a
cool fill light, ambient, and a small rim term.

**Overlays.** Lines and points are rasterised with a distance-based coverage
test against the same depth buffer, with a bias pulling them toward the camera so
a wireframe survives the depth test against the surface it lies on. A larger bias
lets the control cage show through the surface deliberately.

**Anti-aliasing.** The scene renders into a `w·ss × h·ss` target and is
box-filtered into the `w × h` canvas. The HUD is drawn after the resolve, at
native resolution, so text is never softened. `ss = 1` interactively, `ss = 3`
for documentation.

**Viewports.** A `Viewport` remaps the viewport's NDC cube onto the full target
(`x' = x·vw/W + (2vx+vw)/W − 1`) and sets a scissor rectangle. Compare and Levels
modes build four `Renderer` instances against one `RenderTarget`.

## 7. Text and PNG without libraries

- `tools/genfont.py` rasterises Inter and DejaVu Sans Mono into 8-bit alpha
  glyph atlases with per-glyph metrics and emits `src/font_data.h` (base64 blob
  plus a metrics table). The header is committed, so building the project never
  needs Python or Pillow.
- `png.cpp` implements DEFLATE (LZ77 with a 32 KB window and hash chains, fixed
  Huffman coding), Adler-32, CRC-32, adaptive per-scanline PNG filtering, and the
  chunk layout. Output was verified byte-identical against Pillow, with about
  7× compression on rendered frames.

## 8. Window without headers

`x11window.cpp` declares the Xlib entry points and the handful of public structs
it needs (`XKeyEvent`, `XButtonEvent`, `XConfigureEvent`, `XClientMessageEvent`,
`XImage`) and resolves them with `dlopen`/`dlsym`. Pixels reach the screen by
filling an `XImage` by hand, calling `XInitImage`, and `XPutImage`; that avoids
`XCreateImage`/`XDestroyImage` and keeps ownership of the buffer. If the library
or the display is missing, `open()` fails with a message pointing at `--shot`
and the rest of the program is unaffected.

## 9. Risks and mitigations

| Risk | Mitigation |
|---|---|
| A scheme silently produces a broken mesh | Euler characteristic checked for 4 schemes × 10 cages × 3 levels in the self test; non-manifold edges detected in `buildTopology` |
| Inconsistent winding after refinement | Directed-edge audit during development; the E-face fix in §3.3 came from exactly this |
| Subdivision freezes the machine | Face budget in the driver; levels capped in the UI |
| Software rendering too slow to interact | Supersampling defaults to 1 in the window; measured 10–60 ms per frame at 1440×880 |
| Xlib ABI assumptions wrong | Verified by capturing the live window and comparing against the offscreen render of the same state |
| Screenshots drift from the code | `--gallery` regenerates all of them from the app; recipes live in `main.cpp` |
