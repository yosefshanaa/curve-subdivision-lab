# TODO — Interactive Curve Subdivision Lab

Ordered task list. **Milestones 0 + 1 + 2 = the complete deliverable.** Milestone 3 is optional polish — stop after M2 if time or interest runs out; the project is already complete (Milestone 0's submission tasks still apply).

---

## Milestone 0 — Course workflow setup (before any code)

The lecturer grades the report and the git history, not just the app (see PRD §9).

- [x] 0.1 `git init` in the project folder (or clone the course-provided mini-project repo if one is issued via GitHub Classroom — check first); first commit contains PRD.md, PLAN.md, TODO.md.
- [x] 0.2 Create a stub `REPORT.md` now (title, goal, empty sections for each milestone) so screenshots/notes get added as work happens, not reconstructed at the end.
- [x] 0.3 Adopt the commit cadence: **commit after every completed task below** (the lecturer explicitly treats one giant final commit as suspicious). Push, don't just commit locally — the lecturer only sees pushed history.

---

## Milestone 1 — Core (canvas, editing, both schemes with defaults)

### Skeleton
- [x] 1.1 Create `index.html` with page layout: title bar, flex row (canvas area + fixed sidebar), hint bar; dark theme CSS in a `<style>` block.
- [x] 1.2 Canvas setup: size to container, `devicePixelRatio` backing-store scaling, `resize` handler that re-applies the transform and re-renders.
- [x] 1.3 Define the `state` object (PLAN §2) and the `update()` function shell (recompute derived → render → stats), plus an empty `render()` that clears the canvas.

### Point editing
- [x] 1.4 Render control points + control polygon (respecting `state.closed`).
- [x] 1.5 `mousedown`: hit-test (10 px radius, backwards scan); hit → start drag; miss → add point and start dragging it.
- [x] 1.6 `mousemove`/`mouseup`/`mouseleave`: drag moves the point live; hover changes the cursor; highlight hovered/dragged point.
- [x] 1.7 `contextmenu`: prevent default, delete point under cursor.
- [x] 1.8 Sidebar: open/closed toggle, Clear button, Preset-shape button (hardcoded irregular polygon scaled to canvas). The preset also loads automatically on startup — the app must never open to a blank canvas (FR-5).

### Schemes (fixed default weights for now)
- [x] 1.9 Write the `pt(i)` index resolver (wrap when closed; phantom reflection `2·P_0 − P_1` / `2·P_{n−1} − P_{n−2}` when open) — shared by both schemes.
- [x] 1.10 `chaikinStep(points, closed, t)` with t = 0.25: two points per edge; open mode prepends/appends original endpoints (anchoring).
- [x] 1.11 `fourPointStep(points, closed, w)` with w = 1/16: keep old points, insert `Q = (1/2+w)(P_i+P_{i+1}) − w(P_{i−1}+P_{i+2})` per edge via `pt(i)`.
- [x] 1.12 `subdivide()` driver: apply active scheme `iterations` times with the 20,000-vertex cap; store `curve` and `effectiveIterations` in state. Hardcode iterations = 4 for now.
- [x] 1.13 Render the subdivided curve (bold) under the control-polygon overlay; scheme radio buttons in sidebar switch between the two, labeled with lecture vocabulary: "Chaikin — corner cutting (approximating)" / "Four-Point (interpolating)" (FR-6).
- [x] 1.14 Min-point guards: too few points → draw what exists + "add more points" hint instead of running the scheme.
- [x] 1.15 **Console sanity check:** Chaikin on unit square (t=0.25) gives the known 8 points; Four-Point with w=0 gives exact midpoints; open 3-point "V" at w=1/16 gives a smooth arc through all 3 points. *(Done in Node against the functions extracted from index.html — all pass.)*

**M1 done when:** you can draw/edit a polygon and see either scheme's smoothed curve with default weights, open or closed, without errors. ✅

---

## Milestone 2 — Interactivity (sliders, overlay, fractal, stats)

- [x] 2.1 Iterations slider (0–6) wired to state; iteration 0 draws only the full-brightness polygon.
- [x] 2.2 Contextual weight slider: shows/edits `chaikinT` ∈ [0.05, 0.45] when Chaikin active, `fourPointW` ∈ [0, 0.25] when Four-Point active; label, range, and displayed value swap on scheme change; default value marked ("default" reset button).
- [x] 2.3 Overlay toggle (on by default): faint dashed control polygon + point dots drawn on top of the curve.
- [x] 2.4 "Show fractal behavior" preset: sets Four-Point, w = 0.18, iterations = 6. The explanatory caption ("weights far from 1/16 → limit curve is fractal, not smooth") is **state-driven**: shown iff scheme is Four-Point and w > 1/8, so it also appears/disappears correctly when the slider is moved by hand (FR-12).
- [x] 2.5 Stats panel: scheme name + (approximating/interpolating), iterations (with "capped" note when `effectiveIterations < iterations`), current t/w, control-point count, curve vertex count, and **max curve-edge length in px** with an "≈ limit curve (edges < 1 px)" badge when it drops below 1 — this is the visible proof of the "~5 iterations suffice" rule (FR-13).
- [x] 2.6 Hint bar text: "click: add · drag: move · right-click: delete".
- [x] 2.7 Manual test pass (checklist below) + fix everything found. *(Run headlessly via Chrome DevTools Protocol, including synthetic mouse events; one bug found and fixed — see REPORT.md.)*
- [x] 2.8 Tidy pass: consistent naming, dead code removed, short header comment in the file stating what it is and the course context.
- [x] 2.9 Fill `REPORT.md` (PRD §9 D2): what was built, how to run it, screenshots of — both schemes on the same polygon, overlay showing interpolating-vs-approximating, fractal mode, stats panel at 5–6 iterations with the limit-curve badge — plus a mapping of features → lecture concepts and problems encountered along the way.
- [x] 2.10 Final push; skim the pushed git history to confirm it tells a believable incremental story (Milestone 0.3). Optionally share the demo on Discord using the lecturer's format: what it is, why it's useful, what concept it demonstrates. *(Discord share left to the author.)*

**M2 done when:** every functional requirement FR-1 … FR-16 in PRD.md passes the checklist, and REPORT.md + git history are submission-ready. **This is the deliverable.** ✅

---

## Milestone 3 — Optional polish (each item independent; pick any or none)

- [x] 3.1 "Auto-grow" button: steps iterations 0 → 6 on a ~400 ms interval through the normal slider path; button becomes "stop" while running (manual slider use also cancels it).
- [x] 3.2 PNG export button (`canvas.toDataURL` + temporary download link).
- [x] 3.3 Side-by-side compare checkbox: render both schemes on the same polygon in two colors + mini legend (second `subdivide()` call + second stroke pass).
- [x] 3.4 Visual refinement: dark palette, point hover/drag highlight, favicon + title, spacing.
- [ ] 3.5 (Only if demoing from a browser without file access) verify it also works when hosted statically. *(Not needed for the planned file:// demo.)*

---

## Pre-demo manual test checklist

Items marked ✅ were verified headlessly (CDP-driven Chrome, synthetic mouse events, zero console errors). Items marked 👤 need a quick human pass before the live demo — they are about feel/appearance, not correctness.

- [x] ✅ Fresh load → preset polygon and curve already visible (no blank canvas); Clear → hint appears; Preset button restores.
- [x] ✅ Add points by clicking; drag moves them (live curve update); right-click deletes — verified with synthetic mouse events. 👤 also try by hand for feel (no dead zones / accidental adds).
- [x] ✅ Toggle open/closed in both schemes — endpoints stay anchored when open (distance to first/last control point < 1e-6); no gaps or spikes at the ends.
- [ ] 👤 Chaikin: t slider from 0.05 → 0.45 morphs the curve smoothly; corners visibly "cut".
- [x] ✅ Four-Point: curve passes through every control point at w = 1/16 (old points are preserved exactly by construction; visually confirmed in screenshots).
- [ ] 👤 Overlay on: Chaikin misses the dots, Four-Point threads them — visible from 2 m away (projector test).
- [x] ✅ Iterations 0 → 6: vertex count doubles each step (≈ doubles for open Four-Point); max-edge halves each step; badge verified at 6 iterations for a dense 30-point polygon (0.82 px); responsive at 6.
- [x] ✅ Fractal preset: one click → jagged curve + caption; w back to 1/16 hides the caption; dragging w high manually also shows it.
- [x] ✅ 2-point and 3-point polygons, open and closed: hint shown or sane output, never a console error.
- [x] ✅ Degenerate input: coincident points, all-collinear points — no crash.
- [x] ✅ Resize/viewport + DPR change mid-session: clicks still land exactly where aimed.
- [x] ✅ Full reload → fresh state, works from `file://`.
