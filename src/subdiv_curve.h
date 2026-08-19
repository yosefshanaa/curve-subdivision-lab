// subdiv_curve.h — curve subdivision, carried over from the 2D prototype and
// lifted into 3D. These are the schemes the surface rules generalise:
//
//   Chaikin      -> Doo-Sabin        (both approximating; both are the
//                                     quadratic B-spline on a boundary)
//   Four-Point   -> Butterfly        (both interpolating; Butterfly's boundary
//                                     rule *is* the Four-Point rule)
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "vecmath.h"

namespace sl {

enum class CurveScheme { Chaikin, FourPoint, Midpoint, Count };

const char* curveSchemeName(CurveScheme s);
const char* curveSchemeKind(CurveScheme s);
const char* curveParamName(CurveScheme s);
bool parseCurveScheme(const std::string& name, CurveScheme& out);
double curveParamDefault(CurveScheme s);
void curveParamRange(CurveScheme s, double& lo, double& hi);

// One refinement step of the control polygon.
std::vector<Vec3> chaikinStep(const std::vector<Vec3>& p, bool closed, double t);
std::vector<Vec3> fourPointStep(const std::vector<Vec3>& p, bool closed, double w);
std::vector<Vec3> midpointStep(const std::vector<Vec3>& p, bool closed, double roughness,
                               uint32_t& rngState);

struct CurveResult {
    std::vector<Vec3> curve;
    int levelsApplied = 0;
    bool cappedByBudget = false;
    double maxEdgeLen = 0;
    double totalLength = 0;
};

CurveResult subdivideCurve(const std::vector<Vec3>& control, bool closed, CurveScheme scheme,
                           double param, int levels, uint32_t seed, int vertexBudget = 200000);

// Demo control polygons.
std::vector<Vec3> presetCurve(int which, bool& closed);
int presetCurveCount();
const char* presetCurveName(int which);

}  // namespace sl
