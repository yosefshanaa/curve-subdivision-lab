#include "subdiv_curve.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace sl {

const char* curveSchemeName(CurveScheme s) {
    switch (s) {
        case CurveScheme::Chaikin:   return "Chaikin corner cutting";
        case CurveScheme::FourPoint: return "Four-Point";
        case CurveScheme::Midpoint:  return "Midpoint displacement";
        default:                     return "?";
    }
}

const char* curveSchemeKind(CurveScheme s) {
    switch (s) {
        case CurveScheme::Chaikin:   return "approximating";
        case CurveScheme::FourPoint: return "interpolating";
        case CurveScheme::Midpoint:  return "random / fractal";
        default:                     return "?";
    }
}

const char* curveParamName(CurveScheme s) {
    switch (s) {
        case CurveScheme::Chaikin:   return "cut ratio t";
        case CurveScheme::FourPoint: return "weight w";
        case CurveScheme::Midpoint:  return "roughness r";
        default:                     return "?";
    }
}

double curveParamDefault(CurveScheme s) {
    switch (s) {
        case CurveScheme::Chaikin:   return 0.25;      // the classic 1:3 cut
        case CurveScheme::FourPoint: return 1.0 / 16;  // the smooth weight
        case CurveScheme::Midpoint:  return 0.25;
        default:                     return 0;
    }
}

void curveParamRange(CurveScheme s, double& lo, double& hi) {
    switch (s) {
        case CurveScheme::Chaikin:   lo = 0.05; hi = 0.45; break;
        case CurveScheme::FourPoint: lo = 0.0;  hi = 0.25; break;
        default:                     lo = 0.0;  hi = 0.8;  break;
    }
}

bool parseCurveScheme(const std::string& name, CurveScheme& out) {
    std::string n;
    for (char c : name) if (c != '-' && c != '_' && c != ' ') n += char(std::tolower(c));
    if (n == "chaikin")                          { out = CurveScheme::Chaikin; return true; }
    if (n == "fourpoint" || n == "4point")       { out = CurveScheme::FourPoint; return true; }
    if (n == "midpoint" || n == "displacement")  { out = CurveScheme::Midpoint; return true; }
    return false;
}

namespace {

// Index resolver shared by both schemes: wrap when closed, reflect a phantom
// point (P_-1 = 2*P_0 - P_1) when open, so one formula covers every edge.
Vec3 at(const std::vector<Vec3>& p, int i, bool closed) {
    const int n = int(p.size());
    if (closed) return p[((i % n) + n) % n];
    if (i < 0)  return p[0] * 2.0 - p[1];
    if (i >= n) return p[n - 1] * 2.0 - p[n - 2];
    return p[i];
}

inline uint32_t xorshift32(uint32_t& s) {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}
inline double rand11(uint32_t& s) {  // uniform in [-1, 1)
    return (double(xorshift32(s)) / 2147483648.0) - 1.0;
}

}  // namespace

// Each edge (A,B) is replaced by two points at parameter t and 1-t. Corners get
// cut, so the curve pulls away from the control polygon: approximating.
std::vector<Vec3> chaikinStep(const std::vector<Vec3>& p, bool closed, double t) {
    const int n = int(p.size());
    std::vector<Vec3> out;
    if (n < 2) return p;
    t = clampd(t, 0.001, 0.499);
    out.reserve(size_t(n) * 2);

    if (!closed) out.push_back(p[0]);            // anchor the open endpoints
    const int last = closed ? n : n - 1;
    for (int i = 0; i < last; i++) {
        const Vec3& A = p[i];
        const Vec3& B = p[(i + 1) % n];
        out.push_back(A * (1 - t) + B * t);
        out.push_back(A * t + B * (1 - t));
    }
    if (!closed) out.push_back(p[n - 1]);
    return out;
}

// Old points are kept and one new point is inserted per edge:
//   Q = (1/2 + w)(P_i + P_i+1) - w(P_i-1 + P_i+2)
// w = 1/16 gives a smooth limit curve; pushing w well past 1/8 makes it fractal.
std::vector<Vec3> fourPointStep(const std::vector<Vec3>& p, bool closed, double w) {
    const int n = int(p.size());
    if (n < 2) return p;
    std::vector<Vec3> out;
    out.reserve(size_t(n) * 2);

    const int last = closed ? n : n - 1;
    for (int i = 0; i < last; i++) {
        out.push_back(p[i]);
        Vec3 q = (at(p, i, closed) + at(p, i + 1, closed)) * (0.5 + w)
               - (at(p, i - 1, closed) + at(p, i + 2, closed)) * w;
        out.push_back(q);
    }
    if (!closed) out.push_back(p[n - 1]);
    return out;
}

// Keep every old point, insert each edge's midpoint displaced perpendicular to
// the edge by a random amount within +/- r*|edge|/2. The range halves each
// level, which is exactly the procedural-terrain recipe in 1D.
std::vector<Vec3> midpointStep(const std::vector<Vec3>& p, bool closed, double roughness,
                               uint32_t& rngState) {
    const int n = int(p.size());
    if (n < 2) return p;
    std::vector<Vec3> out;
    out.reserve(size_t(n) * 2);

    const int last = closed ? n : n - 1;
    for (int i = 0; i < last; i++) {
        const Vec3& A = p[i];
        const Vec3& B = p[(i + 1) % n];
        out.push_back(A);

        Vec3 edge = B - A;
        double len = length(edge);
        Vec3 dir = (len > 1e-12) ? edge / len : Vec3(1, 0, 0);
        // Any unit vector perpendicular to the edge works; pick a stable one.
        Vec3 up = (std::fabs(dir.y) < 0.9) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
        Vec3 nrm = normalize(cross(dir, up));
        Vec3 bit = cross(dir, nrm);

        double a = rand11(rngState) * roughness * len * 0.5;
        double b = rand11(rngState) * roughness * len * 0.5;
        out.push_back((A + B) * 0.5 + nrm * a + bit * b);
    }
    if (!closed) out.push_back(p[n - 1]);
    return out;
}

CurveResult subdivideCurve(const std::vector<Vec3>& control, bool closed, CurveScheme scheme,
                           double param, int levels, uint32_t seed, int vertexBudget) {
    CurveResult r;
    r.curve = control;
    const int minPts = closed ? 3 : 2;
    if (int(control.size()) < minPts) return r;

    uint32_t rng = seed ? seed : 1u;
    double roughness = param;
    for (int i = 0; i < levels; i++) {
        if (int(r.curve.size()) * 2 > vertexBudget) { r.cappedByBudget = true; break; }
        switch (scheme) {
            case CurveScheme::Chaikin:   r.curve = chaikinStep(r.curve, closed, param); break;
            case CurveScheme::FourPoint: r.curve = fourPointStep(r.curve, closed, param); break;
            case CurveScheme::Midpoint:
                r.curve = midpointStep(r.curve, closed, roughness, rng);
                roughness *= 0.5;   // the displacement range halves each level
                break;
            default: break;
        }
        r.levelsApplied++;
    }

    const int n = int(r.curve.size());
    const int segs = closed ? n : n - 1;
    for (int i = 0; i < segs; i++) {
        double L = length(r.curve[(i + 1) % n] - r.curve[i]);
        r.maxEdgeLen = std::max(r.maxEdgeLen, L);
        r.totalLength += L;
    }
    return r;
}

int presetCurveCount() { return 4; }

const char* presetCurveName(int which) {
    switch (which) {
        case 0: return "Star (closed)";
        case 1: return "Zig-zag (open)";
        case 2: return "Helix cage (open)";
        default: return "Knot (closed)";
    }
}

std::vector<Vec3> presetCurve(int which, bool& closed) {
    std::vector<Vec3> p;
    switch (which % 4) {
        case 0: {   // irregular star: sharp and shallow corners in one shape
            closed = true;
            const int n = 9;
            for (int i = 0; i < n; i++) {
                double a = 2 * PI * i / n;
                double r = (i % 2 == 0) ? 1.0 : 0.45;
                p.push_back(Vec3(r * std::cos(a), 0.35 * std::sin(3 * a), r * std::sin(a)));
            }
            break;
        }
        case 1: {   // open zig-zag: shows endpoint handling
            closed = false;
            for (int i = 0; i < 8; i++)
                p.push_back(Vec3(-1.0 + 2.0 * i / 7.0, (i % 2 ? 0.5 : -0.5), 0.35 * std::sin(i)));
            break;
        }
        case 2: {   // open helix cage
            closed = false;
            const int n = 14;
            for (int i = 0; i < n; i++) {
                double a = 2.4 * PI * i / (n - 1);
                p.push_back(Vec3(0.85 * std::cos(a), -0.9 + 1.8 * i / (n - 1), 0.85 * std::sin(a)));
            }
            break;
        }
        default: {  // closed trefoil-ish knot
            closed = true;
            const int n = 12;
            for (int i = 0; i < n; i++) {
                double a = 2 * PI * i / n;
                p.push_back(Vec3(std::sin(a) + 2 * std::sin(2 * a),
                                 -std::sin(3 * a),
                                 std::cos(a) - 2 * std::cos(2 * a)) * 0.32);
            }
            break;
        }
    }
    return p;
}

}  // namespace sl
