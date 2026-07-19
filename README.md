# Curve Subdivision Lab

An interactive, single-file web app that demonstrates **subdivision of curves** — how a rough control polygon is refined, step by step, toward a smooth limit curve.

Built as a Computer Graphics course mini-project, based on the course's Subdivision lecture. No dependencies, no build step, no server: one HTML file.

![Side-by-side comparison of both schemes](screenshots/07-side-by-side.png)

## Run it

Open [`index.html`](index.html) in any modern desktop browser — double-clicking the file works. A demo polygon loads automatically.

## What it demonstrates

| Lecture concept | Where you see it |
|---|---|
| **Control polygon / control points** | Dashed overlay with draggable dots, always drawn over the curve |
| **Approximating scheme** — Chaikin corner cutting (classic 1:3 cut) | Green curve visibly *misses* the control points, cutting every corner |
| **Interpolating scheme** — Four-Point, `Q = (½+w)(Pᵢ+Pᵢ₊₁) − w(Pᵢ₋₁+Pᵢ₊₂)` | Curve visibly *threads through* every control point |
| **Weights matter — wrong weights go fractal** | Push w far from 1/16 (one-click preset) and the curve stops converging to anything smooth |
| **Limit curve & the "~5 iterations is infinity" rule** | Stats panel: vertex count doubles and max edge length halves each iteration, with an "≈ limit curve" badge once edges drop below 1 px |

### Chaikin (approximating) vs Four-Point (interpolating)

| | |
|---|---|
| ![Chaikin](screenshots/01-chaikin-default.png) | ![Four-Point](screenshots/02-fourpoint-interpolating.png) |

### Fractal mode — the lecture's "wrong weights" warning, one click away

![Fractal behavior](screenshots/04-fractal-mode.png)

## Controls

- **Click** empty canvas — add a control point (and drag it into place in the same gesture)
- **Drag** a point — move it; the curve updates live
- **Right-click** a point — delete it
- **Scheme** — Chaikin (approximating) / Four-Point (interpolating)
- **Iterations** — 0–6 subdivision steps (0 shows just the control polygon)
- **Weight slider** — cut ratio *t* for Chaikin, weight *w* for Four-Point, each with a one-click reset to its classic default (t = 0.25, w = 1/16)
- **Closed polygon** — toggle open polyline vs closed loop
- **Compare both schemes** — render both curves on the same polygon with a legend
- **Show fractal behavior** — preset: Four-Point, w = 0.18, 6 iterations
- **Auto-grow** — animate iterations 0 → 6
- **Export PNG** — save the current canvas

## Implementation notes

- Single `index.html`: state → `update()` → render, no framework, no libraries.
- Both schemes are pure functions behind one driver with a 20,000-vertex safety cap (the lecture warns that over-subdividing freezes machines).
- Open-polyline endpoints: Chaikin re-anchors the original endpoints; Four-Point uses phantom points by reflection (`P₋₁ = 2P₀ − P₁`) so one formula covers all edges and the curve stays interpolating at the ends.
- Scheme math verified numerically in Node; full UI (including synthetic mouse input, degenerate inputs, and HiDPI resize) exercised headlessly via the Chrome DevTools Protocol with zero console errors.

## Project documents

- [PRD.md](PRD.md) — requirements, success criteria, non-goals
- [PLAN.md](PLAN.md) — architecture, the exact scheme math and endpoint strategy, risk list
- [TODO.md](TODO.md) — milestone task list with verification results
- [REPORT.md](REPORT.md) — course report: screenshots, feature → concept mapping, problems encountered
