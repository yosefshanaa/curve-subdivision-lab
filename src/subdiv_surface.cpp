#include "subdiv_surface.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace sl {

const char* schemeName(SurfScheme s) {
    switch (s) {
        case SurfScheme::None:         return "None (control cage)";
        case SurfScheme::CatmullClark: return "Catmull-Clark";
        case SurfScheme::Loop:         return "Loop";
        case SurfScheme::DooSabin:     return "Doo-Sabin";
        case SurfScheme::Butterfly:    return "Modified Butterfly";
        default:                       return "?";
    }
}

const char* schemeShortName(SurfScheme s) {
    switch (s) {
        case SurfScheme::None:         return "none";
        case SurfScheme::CatmullClark: return "catmull-clark";
        case SurfScheme::Loop:         return "loop";
        case SurfScheme::DooSabin:     return "doo-sabin";
        case SurfScheme::Butterfly:    return "butterfly";
        default:                       return "?";
    }
}

const char* schemeKind(SurfScheme s) {
    switch (s) {
        case SurfScheme::None:      return "-";
        case SurfScheme::Butterfly: return "interpolating";
        default:                    return "approximating";
    }
}

const char* schemeFaceKind(SurfScheme s) {
    switch (s) {
        case SurfScheme::CatmullClark: return "quads (any input)";
        case SurfScheme::Loop:         return "triangles";
        case SurfScheme::DooSabin:     return "dual, n-gons";
        case SurfScheme::Butterfly:    return "triangles";
        default:                       return "-";
    }
}

bool schemeNeedsTriangles(SurfScheme s) {
    return s == SurfScheme::Loop || s == SurfScheme::Butterfly;
}

bool parseScheme(const std::string& name, SurfScheme& out) {
    std::string n;
    for (char c : name) if (c != '-' && c != '_' && c != ' ') n += char(std::tolower(c));
    if (n == "none")                                 { out = SurfScheme::None; return true; }
    if (n == "catmullclark" || n == "cc")            { out = SurfScheme::CatmullClark; return true; }
    if (n == "loop")                                 { out = SurfScheme::Loop; return true; }
    if (n == "doosabin" || n == "ds")                { out = SurfScheme::DooSabin; return true; }
    if (n == "butterfly" || n == "modifiedbutterfly"){ out = SurfScheme::Butterfly; return true; }
    return false;
}

namespace {

// Faces incident to v, in CCW order. `closed` reports whether the ring wraps
// all the way round (false => v sits on a boundary).
//
// Crossing the edge (prev_f(v), v) moves CCW around v; crossing (v, next_f(v))
// moves CW. For a boundary vertex we first rewind CW to the start of the fan.
std::vector<int> orderedFacesAroundVertex(const Mesh& m, const Topology& t, int v, bool& closed) {
    closed = false;
    std::vector<int> out;
    if (t.vertFaces[v].empty()) return out;

    auto cornerOf = [&](int f) {
        const std::vector<int>& face = m.F[f];
        for (size_t i = 0; i < face.size(); i++)
            if (face[i] == v) return int(i);
        return -1;
    };
    auto stepCW = [&](int f) {
        int i = cornerOf(f);
        if (i < 0) return -1;
        const std::vector<int>& face = m.F[f];
        int nxt = face[(i + 1) % face.size()];
        int e = t.edgeIndex(v, nxt);
        if (e < 0) return -1;
        return (t.edges[e].f0 == f) ? t.edges[e].f1 : t.edges[e].f0;
    };
    auto stepCCW = [&](int f) {
        int i = cornerOf(f);
        if (i < 0) return -1;
        const std::vector<int>& face = m.F[f];
        int prv = face[(i + face.size() - 1) % face.size()];
        int e = t.edgeIndex(prv, v);
        if (e < 0) return -1;
        return (t.edges[e].f0 == f) ? t.edges[e].f1 : t.edges[e].f0;
    };

    int start = t.vertFaces[v][0];
    size_t guard = t.vertFaces[v].size() + 2;
    for (size_t k = 0; k < guard; k++) {          // rewind to the fan start
        int p = stepCW(start);
        if (p < 0) break;
        if (p == t.vertFaces[v][0]) { closed = true; break; }
        start = p;
    }

    int f = start;
    for (size_t k = 0; k < guard; k++) {
        if (f < 0) break;
        out.push_back(f);
        int nf = stepCCW(f);
        if (nf < 0) break;
        if (nf == start) { closed = true; break; }
        f = nf;
    }
    if (out.size() != t.vertFaces[v].size()) {    // non-manifold safety net
        out = t.vertFaces[v];
        closed = !t.vertBoundary[v];
    }
    return out;
}

}  // namespace

// ============================================================ Catmull-Clark
//
//   face point   F = centroid of the face
//   edge point   E = (P0 + P1 + F_left + F_right) / 4      (boundary: midpoint)
//   vertex point V = (Q + 2R + (n-3)S) / n
//                    Q = mean of adjacent face points
//                    R = mean of adjacent edge midpoints
//                    S = the old vertex, n = its valence
//   boundary vertex: (P_prev + 6P + P_next) / 8   -> a cubic B-spline curve
//   corner  (valence 2 on a boundary): held fixed
Mesh catmullClarkStep(const Mesh& m) {
    Topology t = buildTopology(m);
    const int nV = m.numVerts(), nF = m.numFaces(), nE = t.numEdges();

    Mesh out;
    out.V.resize(size_t(nV) + nE + nF);
    const int VP = 0, EP = nV, FP = nV + nE;   // index bases in the new vertex array

    for (int f = 0; f < nF; f++) out.V[FP + f] = m.faceCentroid(f);

    for (int e = 0; e < nE; e++) {
        const Topology::Edge& ed = t.edges[e];
        Vec3 mid = (m.V[ed.a] + m.V[ed.b]) * 0.5;
        if (ed.boundary()) {
            out.V[EP + e] = mid;
        } else {
            out.V[EP + e] = (m.V[ed.a] + m.V[ed.b] + out.V[FP + ed.f0] + out.V[FP + ed.f1]) * 0.25;
        }
    }

    for (int v = 0; v < nV; v++) {
        const std::vector<int>& ve = t.vertEdges[v];
        const int n = int(ve.size());
        if (n == 0) { out.V[VP + v] = m.V[v]; continue; }

        if (t.vertBoundary[v]) {
            int p, q;
            if (n == 2 || !t.boundaryNeighbours(v, p, q)) {
                out.V[VP + v] = m.V[v];                            // corner: pinned
            } else {
                out.V[VP + v] = (m.V[p] + m.V[v] * 6.0 + m.V[q]) * 0.125;
            }
            continue;
        }

        Vec3 Q(0, 0, 0);
        for (int f : t.vertFaces[v]) Q += out.V[FP + f];
        Q /= double(t.vertFaces[v].size());

        Vec3 R(0, 0, 0);
        for (int e : ve) R += (m.V[t.edges[e].a] + m.V[t.edges[e].b]) * 0.5;
        R /= double(n);

        out.V[VP + v] = (Q + R * 2.0 + m.V[v] * double(n - 3)) / double(n);
    }

    // Every corner of every old face becomes one quad.
    out.F.reserve(size_t(nF) * 4);
    for (int f = 0; f < nF; f++) {
        const std::vector<int>& face = m.F[f];
        const int n = int(face.size());
        for (int i = 0; i < n; i++) {
            int ePrev = t.faceEdges[f][(i + n - 1) % n];   // edge  v[i-1] -> v[i]
            int eNext = t.faceEdges[f][i];                 // edge  v[i]   -> v[i+1]
            out.F.push_back({VP + face[i], EP + eNext, FP + f, EP + ePrev});
        }
    }

    out.computeNormals();
    return out;
}

// ===================================================================== Loop
//
//   odd (new edge) vertex   interior: 3/8(a+b) + 1/8(c+d)
//                           boundary: 1/2(a+b)
//   even (old) vertex       interior: (1-n*beta)v + beta * sum(neighbours),
//                           beta = (1/n)(5/8 - (3/8 + 1/4 cos(2pi/n))^2)
//                           boundary: 3/4 v + 1/8(prev + next)
Mesh loopStep(const Mesh& m) {
    Topology t = buildTopology(m);
    const int nV = m.numVerts(), nE = t.numEdges();

    Mesh out;
    out.V.resize(size_t(nV) + nE);
    const int VP = 0, EP = nV;

    for (int v = 0; v < nV; v++) {
        const std::vector<int>& ve = t.vertEdges[v];
        const int n = int(ve.size());
        if (n == 0) { out.V[v] = m.V[v]; continue; }

        if (t.vertBoundary[v]) {
            int p, q;
            if (n == 2 || !t.boundaryNeighbours(v, p, q)) {
                out.V[v] = m.V[v];
            } else {
                out.V[v] = m.V[v] * 0.75 + (m.V[p] + m.V[q]) * 0.125;
            }
            continue;
        }

        double c = 0.375 + 0.25 * std::cos(2.0 * PI / n);
        double beta = (0.625 - c * c) / n;          // Warren's beta; gives 3/16 at n = 3
        Vec3 sum(0, 0, 0);
        for (int e : ve) sum += m.V[t.otherEnd(e, v)];
        out.V[v] = m.V[v] * (1.0 - n * beta) + sum * beta;
    }

    for (int e = 0; e < nE; e++) {
        const Topology::Edge& ed = t.edges[e];
        if (ed.boundary()) {
            out.V[EP + e] = (m.V[ed.a] + m.V[ed.b]) * 0.5;
        } else {
            int o0, o1;
            if (t.oppositeVerts(m, e, o0, o1) == 2)
                out.V[EP + e] = (m.V[ed.a] + m.V[ed.b]) * 0.375 + (m.V[o0] + m.V[o1]) * 0.125;
            else
                out.V[EP + e] = (m.V[ed.a] + m.V[ed.b]) * 0.5;
        }
    }

    // 1-to-4 triangle split.
    out.F.reserve(m.F.size() * 4);
    for (size_t f = 0; f < m.F.size(); f++) {
        const std::vector<int>& face = m.F[f];
        if (face.size() != 3) continue;
        int v0 = face[0], v1 = face[1], v2 = face[2];
        int e0 = EP + t.faceEdges[f][0];   // v0-v1
        int e1 = EP + t.faceEdges[f][1];   // v1-v2
        int e2 = EP + t.faceEdges[f][2];   // v2-v0
        out.F.push_back({VP + v0, e0, e2});
        out.F.push_back({e0, VP + v1, e1});
        out.F.push_back({e2, e1, VP + v2});
        out.F.push_back({e0, e1, e2});
    }

    out.computeNormals();
    return out;
}

// ================================================================ Doo-Sabin
//
// A dual scheme: each old face shrinks to a smaller copy of itself, and the
// gaps left behind become new faces along old edges and around old vertices.
//
//   alpha_ii = (n + 5) / (4n)
//   alpha_ij = (3 + 2 cos(2 pi (i - j) / n)) / (4n)
//
// On a boundary the new points are joined to Chaikin points (3a+b)/4 and
// (a+3b)/4 of the boundary polyline, so the border converges to the quadratic
// B-spline curve - the surface analogue of Chaikin corner cutting.
Mesh dooSabinStep(const Mesh& m) {
    Topology t = buildTopology(m);
    const int nF = m.numFaces();

    Mesh out;
    // newPoint[f][i] -> index into out.V, for corner i of face f.
    std::vector<std::vector<int>> newPoint(nF);

    for (int f = 0; f < nF; f++) {
        const std::vector<int>& face = m.F[f];
        const int n = int(face.size());
        newPoint[f].resize(n);
        for (int i = 0; i < n; i++) {
            Vec3 p(0, 0, 0);
            for (int j = 0; j < n; j++) {
                double a = (i == j) ? double(n + 5) / (4.0 * n)
                                    : (3.0 + 2.0 * std::cos(2.0 * PI * (i - j) / n)) / (4.0 * n);
                p += m.V[face[j]] * a;
            }
            newPoint[f][i] = int(out.V.size());
            out.V.push_back(p);
        }
    }

    // Boundary Chaikin points, created lazily: chaikinPt[e][0] sits near
    // edges[e].a, chaikinPt[e][1] near edges[e].b.
    std::vector<int> chaikinA(t.numEdges(), -1), chaikinB(t.numEdges(), -1);
    auto chaikinPoint = [&](int e, bool nearA) {
        int& slot = nearA ? chaikinA[e] : chaikinB[e];
        if (slot < 0) {
            const Vec3& A = m.V[t.edges[e].a];
            const Vec3& B = m.V[t.edges[e].b];
            slot = int(out.V.size());
            out.V.push_back(nearA ? (A * 0.75 + B * 0.25) : (A * 0.25 + B * 0.75));
        }
        return slot;
    };

    auto cornerIn = [&](int f, int v) {
        const std::vector<int>& face = m.F[f];
        for (size_t i = 0; i < face.size(); i++)
            if (face[i] == v) return int(i);
        return -1;
    };

    // 1. F-faces: the shrunken copy of every old face.
    for (int f = 0; f < nF; f++) out.F.push_back(newPoint[f]);

    // Does face f traverse this edge in the a -> b direction? Winding the new
    // faces consistently depends on knowing which side of the edge f is on.
    auto traversesAB = [&](int f, int a, int b) {
        const std::vector<int>& face = m.F[f];
        for (size_t i = 0; i < face.size(); i++)
            if (face[i] == a) return face[(i + 1) % face.size()] == b;
        return false;
    };

    // 2. E-faces: one quad per old edge.
    for (int e = 0; e < t.numEdges(); e++) {
        const Topology::Edge& ed = t.edges[e];
        const int a = ed.a, b = ed.b;
        if (ed.f0 < 0) continue;

        if (ed.boundary()) {
            // Close the gap against the boundary curve.
            int f = ed.f0;
            int ia = cornerIn(f, a), ib = cornerIn(f, b);
            if (ia < 0 || ib < 0) continue;
            int pa = chaikinPoint(e, true), pb = chaikinPoint(e, false);
            if (traversesAB(f, a, b))
                out.F.push_back({newPoint[f][ia], pa, pb, newPoint[f][ib]});
            else
                out.F.push_back({newPoint[f][ib], pb, pa, newPoint[f][ia]});
        } else {
            // fAB traverses a -> b, fBA traverses b -> a.
            int fAB = traversesAB(ed.f0, a, b) ? ed.f0 : ed.f1;
            int fBA = (fAB == ed.f0) ? ed.f1 : ed.f0;
            int ia = cornerIn(fAB, a), ib = cornerIn(fAB, b);
            int ja = cornerIn(fBA, a), jb = cornerIn(fBA, b);
            if (ia < 0 || ib < 0 || ja < 0 || jb < 0) continue;
            out.F.push_back({newPoint[fAB][ia], newPoint[fBA][ja],
                             newPoint[fBA][jb], newPoint[fAB][ib]});
        }
    }

    // 3. V-faces: one polygon around every old vertex.
    for (int v = 0; v < m.numVerts(); v++) {
        bool closedRing = false;
        std::vector<int> fan = orderedFacesAroundVertex(m, t, v, closedRing);
        if (fan.empty()) continue;

        std::vector<int> poly;
        if (!closedRing) {
            // Open fan: cap it with the two Chaikin points on the boundary edges.
            // The fan runs CCW, so it opens on the edge leaving v in its first
            // face and closes on the edge entering v in its last face. (At a
            // corner both belong to the same face, so the edges - not the
            // faces - have to disambiguate the two ends.)
            const std::vector<int>& ffirst = m.F[fan.front()];
            const std::vector<int>& flast = m.F[fan.back()];
            int i0 = cornerIn(fan.front(), v), i1 = cornerIn(fan.back(), v);
            if (i0 < 0 || i1 < 0) continue;
            int eStart = t.edgeIndex(v, ffirst[(i0 + 1) % ffirst.size()]);
            int eEnd   = t.edgeIndex(flast[(i1 + flast.size() - 1) % flast.size()], v);
            if (eStart < 0 || eEnd < 0) continue;
            if (!t.edges[eStart].boundary() || !t.edges[eEnd].boundary()) continue;
            poly.push_back(chaikinPoint(eStart, t.edges[eStart].a == v));
            for (int f : fan) {
                int i = cornerIn(f, v);
                if (i >= 0) poly.push_back(newPoint[f][i]);
            }
            poly.push_back(chaikinPoint(eEnd, t.edges[eEnd].a == v));
        } else {
            for (int f : fan) {
                int i = cornerIn(f, v);
                if (i >= 0) poly.push_back(newPoint[f][i]);
            }
        }
        if (poly.size() < 3) continue;
        out.F.push_back(poly);   // the CCW fan order is already the CCW polygon
    }

    out.computeNormals();
    return out;
}

// ================================================ Modified Butterfly (Zorin)
//
// Interpolating: every old vertex survives untouched, so the limit surface
// passes exactly through the control cage - the surface counterpart of the
// Four-Point curve scheme. Only the new edge vertices need a rule.
namespace {

// Regular stencil: both endpoints interior with valence 6.
//   1/2(a+b) + 1/8(c+d) - 1/16(e1+e2+e3+e4)
bool butterflyRegular(const Mesh& m, const Topology& t, int e, Vec3& out) {
    const Topology::Edge& ed = t.edges[e];
    int c, d;
    if (t.oppositeVerts(m, e, c, d) != 2) return false;

    // Wings: the far vertex across each of the four surrounding edges.
    auto wing = [&](int x, int y, int excludeFace) -> int {
        int ei = t.edgeIndex(x, y);
        if (ei < 0) return -1;
        const Topology::Edge& w = t.edges[ei];
        int f = (w.f0 == excludeFace) ? w.f1 : w.f0;
        if (f < 0) return -1;
        for (int vi : m.F[f])
            if (vi != x && vi != y) return vi;
        return -1;
    };
    int fc = -1, fd = -1;
    for (int f : {ed.f0, ed.f1}) {
        if (f < 0) continue;
        for (int vi : m.F[f]) {
            if (vi == c) fc = f;
            if (vi == d) fd = f;
        }
    }
    int w1 = wing(ed.a, c, fc), w2 = wing(c, ed.b, fc);
    int w3 = wing(ed.b, d, fd), w4 = wing(d, ed.a, fd);
    if (w1 < 0 || w2 < 0 || w3 < 0 || w4 < 0) return false;

    out = (m.V[ed.a] + m.V[ed.b]) * 0.5 + (m.V[c] + m.V[d]) * 0.125
        - (m.V[w1] + m.V[w2] + m.V[w3] + m.V[w4]) * 0.0625;
    return true;
}

// Extraordinary stencil centred on v, splitting the edge v-w.
//   Q = 3/4 v + sum_j s_j * ring_j,   ring_0 = w
//   K=3: 5/12, -1/12, -1/12 | K=4: 3/8, 0, -1/8, 0
//   K>=5: s_j = (1/K)(1/4 + cos(2 pi j / K) + 1/2 cos(4 pi j / K))
bool butterflyExtraordinary(const Mesh& m, const Topology& t, int v, int w, Vec3& out) {
    if (t.vertBoundary[v]) return false;
    std::vector<int> ring = t.orderedNeighbours(v);
    const int K = int(ring.size());
    if (K < 3) return false;

    int start = -1;
    for (int i = 0; i < K; i++)
        if (ring[i] == w) { start = i; break; }
    if (start < 0) return false;

    std::vector<double> s(K);
    if (K == 3) {
        s = {5.0 / 12.0, -1.0 / 12.0, -1.0 / 12.0};
    } else if (K == 4) {
        s = {3.0 / 8.0, 0.0, -1.0 / 8.0, 0.0};
    } else {
        for (int j = 0; j < K; j++)
            s[j] = (0.25 + std::cos(2.0 * PI * j / K) + 0.5 * std::cos(4.0 * PI * j / K)) / K;
    }

    Vec3 q = m.V[v] * 0.75;
    for (int j = 0; j < K; j++) q += m.V[ring[(start + j) % K]] * s[j];
    out = q;
    return true;
}

}  // namespace

Mesh butterflyStep(const Mesh& m) {
    Topology t = buildTopology(m);
    const int nV = m.numVerts(), nE = t.numEdges();

    Mesh out;
    out.V.resize(size_t(nV) + nE);
    const int VP = 0, EP = nV;

    for (int v = 0; v < nV; v++) out.V[v] = m.V[v];   // interpolating: kept exactly

    for (int e = 0; e < nE; e++) {
        const Topology::Edge& ed = t.edges[e];
        const int a = ed.a, b = ed.b;
        Vec3 q;

        if (ed.boundary()) {
            // Along a boundary, fall back to the Four-Point curve scheme:
            //   Q = 9/16 (a + b) - 1/16 (a_prev + b_next)
            int pa0, pa1, pb0, pb1;
            int prev = -1, next = -1;
            if (t.boundaryNeighbours(a, pa0, pa1)) prev = (pa0 == b) ? pa1 : pa0;
            if (t.boundaryNeighbours(b, pb0, pb1)) next = (pb0 == a) ? pb1 : pb0;
            if (prev >= 0 && next >= 0)
                q = (m.V[a] + m.V[b]) * 0.5625 - (m.V[prev] + m.V[next]) * 0.0625;
            else
                q = (m.V[a] + m.V[b]) * 0.5;
        } else {
            const bool aReg = !t.vertBoundary[a] && t.valence(a) == 6;
            const bool bReg = !t.vertBoundary[b] && t.valence(b) == 6;
            bool done = false;

            if (aReg && bReg) {
                done = butterflyRegular(m, t, e, q);
            } else if (aReg != bReg) {
                done = butterflyExtraordinary(m, t, aReg ? b : a, aReg ? a : b, q);
            } else {
                Vec3 qa, qb;
                bool ga = butterflyExtraordinary(m, t, a, b, qa);
                bool gb = butterflyExtraordinary(m, t, b, a, qb);
                if (ga && gb)      { q = (qa + qb) * 0.5; done = true; }
                else if (ga)       { q = qa; done = true; }
                else if (gb)       { q = qb; done = true; }
            }

            if (!done) {
                // Near a boundary the full stencil is unavailable; keep the
                // 1/8 opposite-vertex term, drop the wings.
                int o0, o1;
                if (t.oppositeVerts(m, e, o0, o1) == 2)
                    q = (m.V[a] + m.V[b]) * 0.5 + (m.V[o0] + m.V[o1]) * 0.125
                      - (m.V[a] + m.V[b]) * 0.125;
                else
                    q = (m.V[a] + m.V[b]) * 0.5;
            }
        }
        out.V[EP + e] = q;
    }

    out.F.reserve(m.F.size() * 4);
    for (size_t f = 0; f < m.F.size(); f++) {
        const std::vector<int>& face = m.F[f];
        if (face.size() != 3) continue;
        int e0 = EP + t.faceEdges[f][0], e1 = EP + t.faceEdges[f][1], e2 = EP + t.faceEdges[f][2];
        out.F.push_back({VP + face[0], e0, e2});
        out.F.push_back({e0, VP + face[1], e1});
        out.F.push_back({e2, e1, VP + face[2]});
        out.F.push_back({e0, e1, e2});
    }

    out.computeNormals();
    return out;
}

// ================================================================== driver

SubdivResult subdivide(const Mesh& cage, SurfScheme scheme, int levels, int faceBudget) {
    using clock = std::chrono::steady_clock;
    SubdivResult r;
    r.mesh = cage;

    if (schemeNeedsTriangles(scheme) && !cage.allFacesAreTriangles()) {
        r.mesh = triangulateMesh(cage);
        r.triangulatedFirst = true;
    }
    if (r.mesh.faceNormal.size() != r.mesh.F.size()) r.mesh.computeNormals();

    r.faceHistory.push_back(r.mesh.numFaces());
    r.vertHistory.push_back(r.mesh.numVerts());
    if (scheme == SurfScheme::None || levels <= 0) return r;

    auto t0 = clock::now();
    for (int i = 0; i < levels; i++) {
        // Every scheme multiplies the face count by ~4 per level.
        if (r.mesh.numFaces() * 4 > faceBudget) { r.cappedByBudget = true; break; }
        switch (scheme) {
            case SurfScheme::CatmullClark: r.mesh = catmullClarkStep(r.mesh); break;
            case SurfScheme::Loop:         r.mesh = loopStep(r.mesh); break;
            case SurfScheme::DooSabin:     r.mesh = dooSabinStep(r.mesh); break;
            case SurfScheme::Butterfly:    r.mesh = butterflyStep(r.mesh); break;
            default: break;
        }
        r.levelsApplied++;
        r.faceHistory.push_back(r.mesh.numFaces());
        r.vertHistory.push_back(r.mesh.numVerts());
    }
    r.milliseconds = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
    return r;
}

}  // namespace sl
