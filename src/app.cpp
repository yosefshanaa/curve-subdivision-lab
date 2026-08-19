#include "app.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace sl {

const SurfScheme AppState::kCompareSchemes[4] = {
    SurfScheme::CatmullClark, SurfScheme::DooSabin, SurfScheme::Loop, SurfScheme::Butterfly};

const char* modeName(Mode m) {
    switch (m) {
        case Mode::Surface: return "Surface subdivision";
        case Mode::Curve:   return "Curve subdivision";
        case Mode::Terrain: return "Terrain (diamond-square)";
        case Mode::Compare: return "Four schemes compared";
        default:            return "?";
    }
}

bool parseMode(const std::string& s, Mode& out) {
    std::string n;
    for (char c : s) if (c != '-' && c != '_' && c != ' ') n += char(std::tolower(c));
    if (n == "surface") { out = Mode::Surface; return true; }
    if (n == "curve")   { out = Mode::Curve;   return true; }
    if (n == "terrain") { out = Mode::Terrain; return true; }
    if (n == "compare") { out = Mode::Compare; return true; }
    return false;
}

Vec3 schemeColor(SurfScheme s) {
    switch (s) {
        case SurfScheme::CatmullClark: return theme::accent2;
        case SurfScheme::Loop:         return theme::accent;
        case SurfScheme::DooSabin:     return theme::violet;
        case SurfScheme::Butterfly:    return theme::warn;
        default:                       return theme::textDim;
    }
}

std::vector<std::string> schemeRule(SurfScheme s) {
    // Kept ASCII so it renders in the monospace HUD face without surprises.
    switch (s) {
        case SurfScheme::CatmullClark:
            return {"F = centroid of the face",
                    "E = (P0 + P1 + F0 + F1) / 4",
                    "V = (Q + 2R + (n-3)S) / n"};
        case SurfScheme::Loop:
            return {"E = 3/8(a+b) + 1/8(c+d)",
                    "V = (1 - n*b)v + b*sum(nbrs)",
                    "b = (5/8 - (3/8+cos(2pi/n)/4)^2)/n"};
        case SurfScheme::DooSabin:
            return {"corner i of an n-gon:",
                    "  a(i,i) = (n + 5) / 4n",
                    "  a(i,j) = (3+2cos(2pi(i-j)/n))/4n"};
        case SurfScheme::Butterfly:
            return {"old vertices kept exactly",
                    "E = 1/2(a+b) + 1/8(c+d)",
                    "      - 1/16 * (4 wing verts)"};
        default:
            return {"no refinement - the control cage"};
    }
}

int AppState::maxLevel() const {
    switch (mode) {
        case Mode::Curve:   return 8;
        case Mode::Terrain: return 8;
        case Mode::Compare: return 4;
        default:            return 5;
    }
}

double AppState::defaultCamDistance() const {
    return (mode == Mode::Terrain) ? 3.5 : 4.7;
}

void AppState::resetCamera() {
    cam = Camera();
    cam.distance = defaultCamDistance();
    cam.yaw = (mode == Mode::Terrain) ? 0.85 : 0.68;
    cam.pitch = (mode == Mode::Terrain) ? 0.52 : 0.38;
    cam.target = Vec3(0, 0, 0);
}

void AppState::recompute() {
    switch (mode) {
        case Mode::Surface: {
            cage_ = makeBaseMesh(base);
            surf_ = subdivide(cage_, scheme, level);
            Topology ct = buildTopology(cage_);
            cageStats_ = computeStats(cage_, ct);
            Topology st = buildTopology(surf_.mesh);
            surfStats_ = computeStats(surf_.mesh, st);

            // Extraordinary vertices: valence != 4 on a quad mesh, != 6 on a
            // triangle mesh. Their *count* never grows with the level - that is
            // the point worth showing.
            extraordinaryVerts_.clear();
            int regular = surf_.mesh.allFacesAreTriangles() ? 6 : 4;
            for (int v = 0; v < surf_.mesh.numVerts(); v++)
                if (!st.vertBoundary[v] && st.valence(v) != regular)
                    extraordinaryVerts_.push_back(v);
            break;
        }
        case Mode::Curve: {
            control_ = presetCurve(curvePreset, curveClosed_);
            curve_ = subdivideCurve(control_, curveClosed_, curveScheme, curveParam,
                                    curveLevel, curveSeed);
            break;
        }
        case Mode::Terrain: {
            terr_ = makeTerrain(terrain);
            break;
        }
        case Mode::Compare: {
            cage_ = makeBaseMesh(base);
            Topology ct = buildTopology(cage_);
            cageStats_ = computeStats(cage_, ct);
            for (int i = 0; i < 4; i++) {
                cmp_[i] = subdivide(cage_, kCompareSchemes[i], level, 120000);
                Topology t = buildTopology(cmp_[i].mesh);
                cmpStats_[i] = computeStats(cmp_[i].mesh, t);
            }
            break;
        }
        default: break;
    }
}

// --------------------------------------------------------------- rendering

namespace {

Material surfaceMaterial(SurfScheme s) {
    Material m;
    Vec3 c = schemeColor(s);
    // Desaturate the accent into a believable material colour.
    m.albedo = lerp(c, Vec3(0.72, 0.75, 0.82), 0.55) * 0.85;
    m.backAlbedo = m.albedo * 0.35;
    return m;
}

// Longest mesh edge measured in resolved pixels — the 3D analogue of the 2D
// prototype's "edges below one pixel means we have reached the limit" test.
double maxEdgePixels(const Renderer& r, const Mesh& m, int sampleCap = 60000) {
    Topology t = buildTopology(m);
    double worst = 0;
    int n = t.numEdges();
    int stride = std::max(1, n / sampleCap);
    for (int e = 0; e < n; e += stride) {
        double ax, ay, bx, by;
        if (!r.projectToCanvas(m.V[t.edges[e].a], ax, ay)) continue;
        if (!r.projectToCanvas(m.V[t.edges[e].b], bx, by)) continue;
        worst = std::max(worst, std::sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by)));
    }
    return worst;
}

void drawSurfaceScene(Renderer& r, const AppState& st, const Mesh& shown, const Mesh& cage,
                      SurfScheme scheme, bool overlays, double wireThickness) {
    DrawOptions o;
    o.shading = st.shading;
    o.twoSided = true;
    o.backfaceCull = true;

    r.drawMesh(shown, surfaceMaterial(scheme), o);

    if (overlays && st.showWire && shown.numFaces() < 30000)
        r.drawWireframe(shown, theme::wire, wireThickness, 1.2e-3, 0.5);

    if (overlays && st.showCage) {
        r.drawWireframe(cage, theme::cage, 1.5, 1.2e-2, 0.85);
        for (const Vec3& v : cage.V) r.drawPoint3D(v, theme::cagePoint, 3.4, 1.4e-2, 0.95);
    }

    if (overlays && st.showNormals) {
        double len = 0.11;
        for (int f = 0; f < shown.numFaces() && f < 4000; f++) {
            Vec3 c = shown.faceCentroid(f);
            r.drawLine3D(c, c + shown.faceNormal[f] * len, theme::accent2, 1.0, 1e-3, 0.6);
        }
    }
}

}  // namespace

FrameInfo renderFrame(const AppState& st, Canvas& out, int w, int h, int ss) {
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();

    FrameInfo fi;
    RenderTarget rt;
    rt.resize(w, h, ss);
    rt.clear(theme::bgTop, theme::bgBottom);

    // The HUD panel lives on the left, so the 3D viewport starts after it.
    const int gutter = st.showHud ? 348 : 0;

    if (st.mode == Mode::Compare) {
        // 2x2 tiles, one scheme each, all sharing the camera and the cage.
        const int pad = 6;
        const int hintStrip = st.showHud ? 40 : 0;   // leave room for the key hints
        const int vx = gutter + pad, vy = pad;
        const int vw = (w - gutter - pad * 3) / 2, vh = (h - pad * 3 - hintStrip) / 2;
        for (int i = 0; i < 4; i++) {
            Viewport vp{vx + (i % 2) * (vw + pad), vy + (i / 2) * (vh + pad), vw, vh};
            Renderer r(rt, st.cam, vp);
            if (st.showGrid) r.drawGroundGrid(2.6, 10, -1.35, theme::grid, 0.35);
            drawSurfaceScene(r, st, st.cmp_[i].mesh, st.cage_, AppState::kCompareSchemes[i],
                             true, 0.75);
            fi.triangles += r.trianglesDrawn();
        }
        fi.verts = st.cmp_[0].mesh.numVerts();
        fi.faces = st.cmp_[0].mesh.numFaces();
    } else {
        Viewport vp{gutter, 0, w - gutter, h};
        Renderer r(rt, st.cam, vp);
        if (st.showGrid) r.drawGroundGrid(3.0, 12, -1.4, theme::grid, 0.4);

        if (st.mode == Mode::Surface) {
            drawSurfaceScene(r, st, st.surf_.mesh, st.cage_, st.scheme, true, 0.9);
            if (st.showExtraordinary)
                for (int v : st.extraordinaryVerts_)
                    r.drawPoint3D(st.surf_.mesh.V[v], theme::warn, 4.2, 6e-3, 1.0);
            fi.maxEdgePx = maxEdgePixels(r, st.surf_.mesh);
            fi.verts = st.surf_.mesh.numVerts();
            fi.faces = st.surf_.mesh.numFaces();
        } else if (st.mode == Mode::Terrain) {
            DrawOptions o;
            o.shading = st.shading;
            o.twoSided = true;
            Material mat;
            mat.useVertexColor = st.terrain.colorByElevation;
            mat.albedo = rgb(0x7E8B6E);
            r.drawMesh(st.terr_.mesh, mat, o);
            if (st.showWire && st.terr_.mesh.numFaces() < 20000)
                r.drawWireframe(st.terr_.mesh, rgb(0x10161A), 0.7, 1.2e-3, 0.35);
            fi.verts = st.terr_.mesh.numVerts();
            fi.faces = st.terr_.mesh.numFaces();
        } else {  // Curve
            const std::vector<Vec3>& cv = st.curve_.curve;
            Vec3 col = (st.curveScheme == CurveScheme::Chaikin)     ? theme::accent
                     : (st.curveScheme == CurveScheme::FourPoint)   ? theme::accent2
                                                                    : theme::violet;
            r.drawPolyline3D(cv, st.curveClosed_, col, 2.6, 2e-3, 1.0);
            if (st.showCage) {
                r.drawPolyline3D(st.control_, st.curveClosed_, theme::cage, 1.2, 8e-3, 0.75);
                for (const Vec3& p : st.control_)
                    r.drawPoint3D(p, theme::cagePoint, 4.0, 1e-2, 1.0);
            }
            // Longest drawn segment, in pixels.
            const int n = int(cv.size());
            const int segs = st.curveClosed_ ? n : n - 1;
            for (int i = 0; i < segs; i++) {
                double ax, ay, bx, by;
                if (!r.projectToCanvas(cv[i], ax, ay)) continue;
                if (!r.projectToCanvas(cv[(i + 1) % n], bx, by)) continue;
                fi.maxEdgePx = std::max(fi.maxEdgePx,
                                        std::sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by)));
            }
            fi.verts = n;
            fi.faces = 0;
        }
        fi.triangles = r.trianglesDrawn();
    }

    rt.resolveTo(out);
    fi.renderMs = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
    if (st.showHud) drawHud(out, st, fi);
    return fi;
}

// ----------------------------------------------------------------- input

bool handleKey(AppState& st, int ch, bool& quit) {
    quit = false;
    const int maxL = st.maxLevel();
    auto clampLevel = [&](int& v) { v = int(clampd(v, 0, maxL)); };

    switch (ch) {
        case 27: case 'q': case 'Q': quit = true; return false;

        case '1': st.mode = Mode::Surface; st.resetCamera(); st.recompute(); return true;
        case '2': st.mode = Mode::Curve;   st.resetCamera(); st.recompute(); return true;
        case '3': st.mode = Mode::Terrain; st.resetCamera(); st.recompute(); return true;
        case '4': st.mode = Mode::Compare; st.resetCamera(); st.recompute(); return true;

        case 's':
            if (st.mode == Mode::Curve) {
                st.curveScheme = CurveScheme(((int(st.curveScheme) + 1) % int(CurveScheme::Count)));
                st.curveParam = curveParamDefault(st.curveScheme);
            } else {
                do {
                    st.scheme = SurfScheme((int(st.scheme) + 1) % int(SurfScheme::Count));
                } while (st.scheme == SurfScheme::None);
            }
            st.recompute();
            return true;
        case 'S':
            if (st.mode == Mode::Curve) {
                st.curveScheme = CurveScheme((int(st.curveScheme) + int(CurveScheme::Count) - 1) %
                                             int(CurveScheme::Count));
                st.curveParam = curveParamDefault(st.curveScheme);
            } else {
                do {
                    st.scheme = SurfScheme((int(st.scheme) + int(SurfScheme::Count) - 1) %
                                           int(SurfScheme::Count));
                } while (st.scheme == SurfScheme::None);
            }
            st.recompute();
            return true;

        case 'm':
            if (st.mode == Mode::Curve) st.curvePreset = (st.curvePreset + 1) % presetCurveCount();
            else st.base = BaseMesh((int(st.base) + 1) % int(BaseMesh::Count));
            st.recompute();
            return true;
        case 'M':
            if (st.mode == Mode::Curve)
                st.curvePreset = (st.curvePreset + presetCurveCount() - 1) % presetCurveCount();
            else
                st.base = BaseMesh((int(st.base) + int(BaseMesh::Count) - 1) % int(BaseMesh::Count));
            st.recompute();
            return true;

        case '+': case '=': case ']':
            if (st.mode == Mode::Curve)        { st.curveLevel++;   clampLevel(st.curveLevel); }
            else if (st.mode == Mode::Terrain) { st.terrain.levels++; st.terrain.levels = int(clampd(st.terrain.levels, 1, maxL)); }
            else                               { st.level++;        clampLevel(st.level); }
            st.recompute();
            return true;
        case '-': case '_': case '[':
            if (st.mode == Mode::Curve)        { st.curveLevel--;   clampLevel(st.curveLevel); }
            else if (st.mode == Mode::Terrain) { st.terrain.levels--; st.terrain.levels = int(clampd(st.terrain.levels, 1, maxL)); }
            else                               { st.level--;        clampLevel(st.level); }
            st.recompute();
            return true;

        case ',': case '<': {
            if (st.mode == Mode::Curve) {
                double lo, hi;
                curveParamRange(st.curveScheme, lo, hi);
                st.curveParam = clampd(st.curveParam - (hi - lo) / 40.0, lo, hi);
            } else if (st.mode == Mode::Terrain) {
                st.terrain.roughness = clampd(st.terrain.roughness - 0.05, 0.0, 1.0);
            }
            st.recompute();
            return true;
        }
        case '.': case '>': {
            if (st.mode == Mode::Curve) {
                double lo, hi;
                curveParamRange(st.curveScheme, lo, hi);
                st.curveParam = clampd(st.curveParam + (hi - lo) / 40.0, lo, hi);
            } else if (st.mode == Mode::Terrain) {
                st.terrain.roughness = clampd(st.terrain.roughness + 0.05, 0.0, 1.0);
            }
            st.recompute();
            return true;
        }

        case 'e': case 'E':
            st.curveSeed = st.curveSeed * 1664525u + 1013904223u;
            st.terrain.seed = st.terrain.seed * 1664525u + 1013904223u;
            st.recompute();
            return true;

        case 'w': case 'W': st.showWire = !st.showWire; return true;
        case 'c': case 'C': st.showCage = !st.showCage; return true;
        case 'g': case 'G': st.showGrid = !st.showGrid; return true;
        case 'x': case 'X': st.showExtraordinary = !st.showExtraordinary; return true;
        case 'n': case 'N': st.showNormals = !st.showNormals; return true;
        case 'h': case 'H': st.showHud = !st.showHud; return true;
        case 'a': case 'A': st.autoRotate = !st.autoRotate; return true;
        case 'f': case 'F':
            st.shading = Shading((int(st.shading) + 1) % int(Shading::Count));
            return true;
        case 'r': case 'R': st.resetCamera(); return true;

        case 'i': st.cam.orbit(0, 0.08); return true;
        case 'k': st.cam.orbit(0, -0.08); return true;
        case 'j': st.cam.orbit(-0.10, 0); return true;
        case 'l': st.cam.orbit(0.10, 0); return true;
        case 'z': st.cam.zoom(0.92); return true;
        case 'Z': st.cam.zoom(1.08); return true;
        default: return false;
    }
}

std::string statusLine(const AppState& st) {
    char buf[256];
    switch (st.mode) {
        case Mode::Surface:
            std::snprintf(buf, sizeof buf, "%s | %s | %s | level %d | %d faces",
                          modeName(st.mode), baseMeshName(st.base), schemeName(st.scheme),
                          st.surf_.levelsApplied, st.surf_.mesh.numFaces());
            break;
        case Mode::Curve:
            std::snprintf(buf, sizeof buf, "%s | %s | %s | level %d | %d verts",
                          modeName(st.mode), presetCurveName(st.curvePreset),
                          curveSchemeName(st.curveScheme), st.curve_.levelsApplied,
                          int(st.curve_.curve.size()));
            break;
        case Mode::Terrain:
            std::snprintf(buf, sizeof buf, "%s | level %d | %dx%d grid | roughness %.2f",
                          modeName(st.mode), st.terrain.levels, st.terr_.gridSize,
                          st.terr_.gridSize, st.terrain.roughness);
            break;
        default:
            std::snprintf(buf, sizeof buf, "%s | %s | level %d", modeName(st.mode),
                          baseMeshName(st.base), st.level);
            break;
    }
    return buf;
}

}  // namespace sl
