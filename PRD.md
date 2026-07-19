# PRD — Interactive Curve Subdivision Lab

## 1. Overview

**Product:** A single-page interactive web app that demonstrates subdivision of curves, as taught in the course's Subdivision lecture (part of the "Hidden Surface Removal + Subdivision" lecture).

**Form factor:** One self-contained HTML file (HTML5 Canvas + vanilla JavaScript, zero external dependencies). Open the file in any modern desktop browser and it works — no build step, no server, no install.

**Course context:** This is the mini-project (20% of grade). The lecturer described mini-projects as roughly a one-day extra exercise, so the scope is intentionally small and demo-focused. The lecture itself lists "Subdivision Experiments — change the recipe weights, observe when results become smooth, rough, or fractal-like" as a suggested mini-project direction; this app is exactly that, made interactive.

## 2. Goal

Make the core concepts of curve subdivision *visible and manipulable*:

1. A rough **control polygon** refined step-by-step toward a smooth **limit curve**.
2. The difference between an **approximating** scheme (Chaikin corner cutting) and an **interpolating** scheme (Four-Point).
3. How the **scheme weights** control smoothness — including the lecture's warning that wrong weights produce fractal-like, nowhere-smooth curves.
4. Why **~5 iterations** are "infinity" in practice (edges shrink below one pixel; vertex count grows exponentially).

## 3. Target Users

- **The lecturer**, during a demo/grading session: needs to see the concepts within ~2 minutes of interaction, without instructions.
- **The student (author)**, while presenting: needs controls that are easy to drive live and hard to break.
- Secondary: other students exploring the demo from a shared link/file.

## 4. Success Criteria

- S1: A first-time viewer can draw a polygon and see a smooth curve in under 30 seconds.
- S2: With overlay enabled, the approximating-vs-interpolating difference is visually obvious in one glance (Four-Point curve passes through control points; Chaikin curve pulls away from them).
- S3: Pressing the fractal preset visibly transforms the smooth Four-Point curve into a jagged, self-similar one.
- S4: The stats panel makes the exponential vertex growth explicit (e.g., 8 → 512 vertices at 6 iterations).
- S4b: The stats panel demonstrates the lecture's "~5 iterations is effectively infinity" rule directly: it shows the longest curve-edge length in pixels (visibly halving each iteration), and labels the curve "≈ limit curve" once edges drop below 1 px. Note: how many iterations that takes depends on the initial edge length — a densely clicked polygon (edges ~50 px) reaches sub-pixel at 5–6 iterations; the sparse 9-point preset (edges ~300 px) does not by 6, which is itself a truthful demonstration of the rule's assumption.
- S5: The app never freezes or crashes during a live demo (iteration cap + vertex cap enforced).
- S6: Everything runs from a double-clicked local file with no network access.

## 5. Functional Requirements

Numbered and testable. FR-1 … FR-16 are the deliverable; FR-17 … FR-19 are optional polish.

### Control polygon editing

- **FR-1:** Left-click on empty canvas adds a control point at the cursor, appended to the polygon.
- **FR-2:** Dragging an existing control point moves it; the subdivided curve updates live during the drag.
- **FR-3:** Right-click (or delete mode / double-click fallback) removes the control point under the cursor.
- **FR-4:** A toggle switches between **open** polyline and **closed** polygon. The curve updates immediately and endpoint behavior changes accordingly.
- **FR-5:** A "Clear" button removes all points; a "Preset shape" button loads a sensible demo polygon. **The preset loads automatically on startup**, so the app never opens to a blank screen.

### Subdivision schemes

- **FR-6:** The user can select the active scheme. The UI labels use the lecture's vocabulary: **"Chaikin — corner cutting (approximating)"** and **"Four-Point (interpolating)"**, so the classification is on screen at all times.
- **FR-7:** Chaikin implements corner cutting: each edge is replaced by two new points at parameter `t` and `1−t` along the edge. Default `t = 0.25` (the classic 1:3 cut mentioned in the lecture). A slider adjusts `t` in [0.05, 0.45], with the default marked and a one-click reset to it.
- **FR-8:** Four-Point inserts, for each edge (P_i, P_{i+1}), a new point
  `Q = (1/2 + w)(P_i + P_{i+1}) − w(P_{i−1} + P_{i+2})`
  keeping all old points (interpolating). Default `w = 1/16`. A slider adjusts `w` in [0, 0.25], with the default marked and a one-click reset to it.
- **FR-9:** An iterations slider selects 0–6 subdivision iterations. 0 shows only the control polygon.
- **FR-10:** Open polylines are handled correctly at endpoints for both schemes (strategy specified in PLAN.md): endpoints of the curve stay anchored to the first/last control points, and no scheme reads out-of-range indices.

### Visualization

- **FR-11:** The subdivided curve is drawn prominently; the original control polygon and its control points are drawn faintly on top (toggleable overlay, on by default). This makes S2 achievable.
- **FR-12:** A "Show fractal behavior" preset button switches to Four-Point, sets `w` to a value well above 1/16 (e.g., 0.18), and sets iterations high (e.g., 6), so the fractal effect appears with one click. A caption ("w far from 1/16 breaks smoothness — the limit curve becomes fractal") is **state-driven**: it is visible whenever the active scheme is Four-Point and `w > 1/8`, and disappears when `w` returns to the smooth range — regardless of whether the preset button or the slider got it there.
- **FR-13:** A live stats panel shows: active scheme, iteration count, current weight value, number of control points, number of curve vertices after subdivision, and the **longest curve-edge length in pixels**. When that length drops below 1 px, the panel shows an "≈ limit curve (edges < 1 px)" badge — this is the app's concrete demonstration of the lecture's "~5 iterations reach sub-pixel edges" rule.

### Robustness

- **FR-14:** With too few points to subdivide (both schemes: fewer than 2 points when open, fewer than 3 when closed), the app degrades gracefully: it draws whatever points/segments exist and shows a hint ("add more points") instead of erroring.
- **FR-15:** A hard cap on total curve vertices (e.g., 20,000) silently limits effective iterations for very large control polygons — the lecture warns that over-subdividing freezes machines.
- **FR-16:** The canvas resizes with the window without losing state.

### Optional polish (Milestone 3 — allowed, not required)

- **FR-17:** Animated transition when the iteration count changes (brief interpolation between levels or a stepped auto-play "grow" button).
- **FR-18:** Export the current canvas as a PNG (`canvas.toDataURL`, one button).
- **FR-19:** Side-by-side mode: both schemes rendered simultaneously on the same control polygon in two colors, with a legend.

## 6. Non-Goals / Out of Scope

Explicitly excluded to keep the project at the "1–2 days" complexity the lecturer expects:

- **No surface subdivision** (Catmull-Clark, Loop, Doo-Sabin, Butterfly). Curves only. Surfaces can be mentioned verbally in the demo as "the 3D generalization."
- **No 3D rendering**, no WebGL/WebGPU, no camera.
- **No backend**, no persistence, no accounts, no URL state sharing.
- **No mobile/touch support.** Desktop mouse interaction only.
- **No external libraries or fonts.** One file, zero dependencies.
- **No mathematical analysis features** (curvature plots, convergence proofs, limit-curve error metrics).
- **No undo/redo.** Clear + preset button is enough for a demo.
- **No cross-browser QA matrix.** Works in current Chrome/Firefox; anything else is best-effort.

## 7. UI Layout

Single screen, no navigation:

```
+---------------------------------------------------------------+
|  Curve Subdivision Lab                                        |
+-----------------------------------------+---------------------+
|                                         | CONTROLS (sidebar)  |
|                                         |                     |
|                                         | Scheme:  (o) Chaikin|
|                                         |          ( ) 4-Point|
|                CANVAS                   |                     |
|                                         | Iterations  [0──6]  |
|   • control points (faint, draggable)   | Weight/Ratio [────] |
|   ─ control polygon (faint)             |                     |
|   ━ subdivided curve (bold)             | [x] Closed polygon  |
|                                         | [x] Show overlay    |
|                                         |                     |
|                                         | [Show fractal]      |
|                                         | [Preset shape]      |
|                                         | [Clear]             |
|                                         +---------------------+
|                                         | STATS               |
|                                         | Scheme: Chaikin     |
|                                         |  (approximating)    |
|                                         | Iterations: 4       |
|                                         | Control pts: 8      |
|                                         | Curve verts: 128    |
|                                         | Max edge: 3.2 px    |
+-----------------------------------------+---------------------+
|  hint bar: "click: add · drag: move · right-click: delete"    |
+---------------------------------------------------------------+
```

- Canvas takes all remaining width; sidebar is fixed-width (~260 px).
- The weight slider is contextual: it shows the cut ratio `t` when Chaikin is active and the weight `w` when Four-Point is active, each with its default marked.
- Colors: dark canvas background, bright curve, dimmed gray control polygon — chosen for projector visibility.

## 8. Assumptions

- A1: The app will be demoed on a desktop browser from a local file; no hosting is required.
- A2: The "Subdivision" lecture content as captured in the course notes (control polygon / control points / limit curve, Chaikin ≈ 1:3 corner cutting, interpolating Four-Point, fractal behavior from wrong weights, ~5 iterations rule) is the authoritative scope; concepts not in the lecture are out of scope.
- A3: Six iterations is a sufficient upper bound: it exceeds the lecture's "~5 is effectively infinity" rule and demonstrates the point without risking performance (with the FR-15 cap).
- A4: A single default preset polygon (an irregular star-ish shape with both sharp and shallow corners) is enough to demonstrate all behaviors.
- A5: English UI text.

## 9. Course Submission Requirements

The app itself is not the whole deliverable. Per the course's stated workflow (emphasized repeatedly in the lecture notes):

- **D1 — Git history:** the project lives in a git repository with incremental commits (at least one per meaningful task/milestone). The lecturer explicitly treats a single final "everything" commit as suspicious.
- **D2 — Report:** a short `REPORT.md` with real content, not filler: what was built, screenshots of each key behavior (both schemes, overlay comparison, fractal mode, stats at high iterations), which lecture concepts each feature demonstrates, and problems encountered. The lecturer grades primarily from the report.
- **D3 — Demo context (if shared on Discord):** follow the lecturer's stated format — what the demo is, why it is useful, and what concept it demonstrates.

These are tracked as first-class tasks in TODO.md, not afterthoughts.
