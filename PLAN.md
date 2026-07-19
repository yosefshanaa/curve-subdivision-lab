# PLAN — Interactive Curve Subdivision Lab

Technical plan for the app specified in PRD.md. Single developer, single file, no dependencies.

## 1. Architecture

One file: `index.html`. Inside it, one `<style>` block, one `<canvas>`, a sidebar `<div>`, and one `<script>` organized into five plain-object modules (no classes needed at this size, no bundler):

```
index.html
└── <script>
    ├── state      — the single source of truth (app state object)
    ├── schemes    — pure geometry: chaikin(), fourPoint(), subdivide()
    ├── render     — draws state onto the canvas (the only code that touches ctx)
    ├── input      — mouse handlers on the canvas (add/drag/delete points)
    └── ui         — binds sliders/buttons/toggles to state, updates stats panel
```

**Data flow (unidirectional):** input/ui mutate `state` → call `update()` → `update()` recomputes the subdivided curve (schemes) and calls `render()` + refreshes stats. There is no other path to the screen. This keeps the app trivially debuggable: any visual bug is either in `subdivide()` output (log the array) or in `render()`.

No animation loop by default — the canvas redraws only on change (event-driven). A `requestAnimationFrame` loop is introduced only if Milestone 3's animated transitions are built.

## 2. Data Model

```js
const state = {
  // geometry
  points: [ {x, y}, ... ],   // control points, in insertion order
  closed: true,              // closed polygon vs open polyline

  // scheme settings
  scheme: 'chaikin',         // 'chaikin' | 'fourpoint'
  iterations: 4,             // 0..6
  chaikinT: 0.25,            // cut parameter t, in [0.05, 0.45]
  fourPointW: 1/16,          // weight w, in [0, 0.25]

  // view options
  showOverlay: true,

  // interaction (transient)
  dragIndex: -1,             // index of point being dragged, -1 = none

  // derived (recomputed by update(), never edited directly)
  curve: [ {x, y}, ... ],    // result of subdivision
  effectiveIterations: 4,    // may be < iterations due to vertex cap
  maxEdgeLen: 3.2,           // longest curve edge in CSS px (drives the "≈ limit curve" badge, FR-13)
};
```

Derived data (`curve`, `effectiveIterations`) is recomputed from scratch on every change. With the vertex cap at 20,000 points this is well under a millisecond of arithmetic — no caching or incremental updates needed.

## 3. Scheme Mathematics

Both schemes are pure functions `(points, closed, param) → newPoints`, applied `iterations` times by a shared driver:

```js
function subdivide(points, closed, scheme, param, iterations, cap) {
  let pts = points;
  let done = 0;
  while (done < iterations && pts.length * 2 <= cap) {
    pts = (scheme === 'chaikin') ? chaikinStep(pts, closed, param)
                                 : fourPointStep(pts, closed, param);
    done++;
  }
  return { curve: pts, effectiveIterations: done };
}
```

### 3.1 Chaikin corner cutting (approximating)

For each edge (A, B), emit two points at parameter `t` and `1−t`:

```
Q = (1−t)·A + t·B          // near A
R = t·A + (1−t)·B          // near B
```

Default `t = 0.25` gives the classic points at 1/4 and 3/4 — the "1:3" cut from the lecture. Corners get cut off; the curve pulls away from the control points (approximating). Repeated forever, the limit curve is smooth (for t = 1/4 it is the quadratic B-spline curve — worth one sentence in the demo, not implemented as a feature).

**Closed polygon:** iterate over all n edges including (P_{n−1}, P_0). Output 2n points.

**Open polyline:** iterate over the n−1 edges → 2(n−1) points, then **prepend the original first point and append the original last point** so the curve stays anchored at the ends (endpoint-preserving Chaikin). Without this, the curve visibly shrinks away from the endpoints each iteration, which looks like a bug in a demo.

Note: with anchoring, output length is 2(n−1)+2 = 2n, so growth is ×2 per iteration in both modes — keeps the stats panel story simple.

### 3.2 Four-Point scheme (interpolating)

All old points are kept. For each edge (P_i, P_{i+1}) a new midpoint-ish point is inserted using the four surrounding points:

```
Q_i = (1/2 + w)·(P_i + P_{i+1}) − w·(P_{i−1} + P_{i+2})
```

With the classic `w = 1/16` this is the well-known stencil `(−1, 9, 9, −1)/16`. The negative weights push the new point slightly *outward*, which is what lets the limit curve pass smoothly *through* the old points (interpolating) instead of cutting corners.

- `w = 0` → plain midpoint insertion → limit is the control polygon itself (still piecewise-linear). Good teaching moment on the slider.
- `w = 1/16` → smooth (C¹) limit curve.
- `w` well above ~1/8 → the scheme stops converging to a smooth curve; the curve becomes increasingly jagged and self-similar at every zoom level — the lecture's "fractal from wrong weights" warning. The fractal preset uses `w = 0.18`, iterations 6.

**Closed polygon:** indices wrap modulo n. For edge i, the stencil uses `P[(i−1+n)%n], P[i], P[(i+1)%n], P[(i+2)%n]`. Output interleaves old and new: 2n points.

**Growth-rate note:** closed output is 2n points, open output is 2n−1 (n old + n−1 inserted). So "vertex count doubles per iteration" is exact for closed polygons and approximate for open ones — the stats panel just reports the real count, but demo phrasing (and the test checklist) should say "≈ doubles" for the open case.

**Open polyline — endpoint strategy (the risky part, decided now):** edges at the ends lack a `P_{i−1}` or `P_{i+2}`. Chosen strategy: **phantom points by reflection**:

```
P_{−1} = 2·P_0     − P_1        // mirror P_1 through P_0
P_{n}  = 2·P_{n−1} − P_{n−2}    // mirror P_{n−2} through P_{n−1}
```

This linearly extrapolates the polygon past its ends, so the first and last edges use the same formula as interior edges with the phantom substituted. Endpoints remain exactly interpolated (old points are never moved), and the curve leaves the endpoints along the end-edge direction, which looks natural.

Rejected alternatives, for the record:
- *Index clamping* (`P_{−1} = P_0`): simplest, but flattens the curve near the ends — visibly worse.
- *Dropping incomplete edges*: leaves gaps between the curve and the endpoints — breaks the "interpolating" story.

Implementation detail: write one helper `pt(i)` per step that resolves an arbitrary integer index to a real, wrapped, or phantom point depending on `closed`. Then the loop body is identical for both modes — this is where off-by-one bugs would otherwise live.

### 3.3 Minimum-size guards

- Chaikin: needs ≥ 2 points (open) / ≥ 3 (closed). Below that: draw points only, show hint.
- Four-Point: needs ≥ 2 points open (phantoms cover the rest) / ≥ 3 closed. Same fallback.
- Duplicate/coincident points are harmless in both schemes (no division anywhere) — no special handling.

## 4. Rendering

Painter's order (back to front) on each `render()` call:

1. Clear canvas (dark background, e.g., `#1a1a24`).
2. **Subdivided curve** — bold stroke (~2.5 px, bright accent color), built with one `Path2D`/`beginPath` pass; `closePath()` iff closed.
3. **Overlay** (if `showOverlay`): control polygon as thin dashed gray line; control points as small filled circles (radius ~5 px), slightly brighter so they're clearly grabbable. Drawn *on top* of the curve so that in Four-Point mode the curve visibly threads through the dots, and in Chaikin mode it visibly misses them — this ordering is what sells requirement S2.
4. Hovered/dragged point highlighted (larger radius / white ring).
5. At iterations 0: only the polygon is drawn, at full brightness (it *is* the curve).

**Canvas sizing:** CSS size from flex layout; backing store scaled by `devicePixelRatio` on init and on `resize` (via `ctx.setTransform(dpr, 0, 0, dpr, 0, 0)`), so lines are crisp on HiDPI screens and mouse coordinates stay in CSS pixels. Points are stored in CSS-pixel coordinates; on window resize the points keep their coordinates (no rescaling — acceptable for a demo, noted in PRD A1).

**Redraw policy:** event-driven only. Slider `input` events, mouse-move during drag, and toggles each call `update()`. No idle repainting.

## 5. Input Handling

All on the canvas element:

| Event | Behavior |
|---|---|
| `mousedown` (left) | Hit-test control points (distance ≤ 10 px, iterate backwards so the topmost/latest wins). Hit → start drag (`dragIndex = i`). Miss → add new point at cursor, immediately start dragging it (nice UX: click-drag-place in one gesture). |
| `mousemove` | Dragging → move point, `update()`. Not dragging → hit-test for hover cursor (`pointer` vs `crosshair`). |
| `mouseup` / `mouseleave` | End drag. |
| `contextmenu` | `preventDefault()`; hit-test; hit → remove that point, `update()`. |

Hit-testing is a linear scan over ≤ a few dozen control points — no spatial structure needed. The hit radius (10 px) is deliberately larger than the drawn radius (5 px) for forgiving grabbing during a live demo.

Sidebar controls are ordinary DOM `input`/`change` listeners in the `ui` module; each writes to `state` and calls `update()`. The `ui` module also owns:
- swapping the contextual slider label/range/value when the scheme changes (t vs w), including the marked default and a "reset to default" affordance (FR-7/FR-8),
- the fractal preset (sets scheme='fourpoint', w=0.18, iterations=6, then `update()`),
- the fractal caption, which is **derived from state, not from the button**: visible iff `scheme === 'fourpoint' && fourPointW > 1/8` (FR-12) — so it also appears when the user drags the slider there manually, and disappears when w returns to the smooth range,
- the shape preset (loads a hardcoded irregular polygon centered/scaled to the current canvas size), **also invoked once at startup** so the app never opens blank (FR-5),
- stats panel refresh (innerText updates from `state` — cheap, done inside `update()`), including `maxEdgeLen` (one pass over `curve` inside `update()`) and the "≈ limit curve (edges < 1 px)" badge when `maxEdgeLen < 1` (FR-13).

## 6. Milestone-3 Notes (only if built)

- **Animated transitions:** on iteration change, linearly interpolate for ~200 ms between the previous curve and the new one *only when one is a refinement of the other* is complex; instead use the simpler, equally convincing approach: an "auto-grow" button that steps iterations 0→6 at 400 ms intervals (a `setInterval` driving the existing slider). No per-vertex correspondence math.
- **PNG export:** `canvas.toDataURL('image/png')` into a temporary `<a download>` click. ~10 lines.
- **Side-by-side:** not two canvases — render both schemes' curves on the *same* canvas in two colors with a small legend. Reuses everything; one extra `subdivide()` call and stroke pass behind a checkbox.

## 7. Risks and Mitigations

| # | Risk | Impact | Mitigation |
|---|---|---|---|
| R1 | Four-Point endpoint handling wrong (off-by-one, bad phantom) → curve detaches from endpoints or spikes at the ends | High — it's the centerpiece scheme | Single `pt(i)` index-resolver helper (§3.2) used by both modes; manual test checklist item: open 3-point "V" polyline must produce a smooth interpolating arc through all 3 points at w=1/16 |
| R2 | Exponential blowup freezes the tab (lecture explicitly warns about this) | High for a live demo | Vertex cap in the `subdivide()` driver (§3), iterations hard-capped at 6, stats panel shows effective iterations when capped |
| R3 | Drag hit-testing feels bad (can't grab points, or click-to-add fires when trying to drag) | Medium — makes demo look janky | Hit radius 10 px > draw radius; hit-test before add; backwards iteration for overlap; add-then-drag gesture |
| R4 | Chaikin open-curve shrinkage read as a bug by the lecturer | Medium | Endpoint anchoring (§3.1), and it becomes a talking point: "pure Chaikin approximates *everywhere*, we pin the ends" |
| R5 | Fractal mode misread as a rendering bug | Low | Preset button + one-line caption ("w far from 1/16 breaks smoothness — the curve converges to a fractal") |
| R6 | Contextual slider (t vs w) confuses state (stale value applied to wrong scheme) | Low | Separate state fields `chaikinT` / `fourPointW`; the slider is only a *view* onto whichever is active |
| R7 | HiDPI/resize coordinate mismatch (clicks land offset from points) | Medium — classic canvas bug | Single dpr-aware transform set in one place; mouse coords taken from `getBoundingClientRect()`; resize handler re-applies transform and re-renders |
| R8 | Scope creep (adding B-spline math, zoom, more schemes) | Medium — violates lecturer's size expectation | PRD non-goals list; Milestone 3 is explicitly optional and last |

## 8. Testing Approach

No test framework (would exceed project scope); a manual checklist run before the demo, kept at the bottom of TODO.md. The pure `schemes` functions are the only logic worth unit-checking; they can be sanity-verified in the browser console (e.g., Chaikin on a unit square with t=0.25 → known 8 points; Four-Point with w=0 → exact midpoints).
