// subdiv_surface.h — surface subdivision schemes.
//
//                        approximating          interpolating
//   quad / general       Catmull-Clark          -
//                        Doo-Sabin (dual)
//   triangle             Loop                   Modified Butterfly
//
// Each scheme is one pure function `Mesh step(const Mesh&)`; `subdivide()`
// applies the chosen one repeatedly under a face budget.
#pragma once

#include <string>
#include <vector>

#include "mesh.h"

namespace sl {

enum class SurfScheme { None, CatmullClark, Loop, DooSabin, Butterfly, Count };

const char* schemeName(SurfScheme s);
const char* schemeShortName(SurfScheme s);
const char* schemeKind(SurfScheme s);      // "approximating" / "interpolating"
const char* schemeFaceKind(SurfScheme s);  // what the scheme produces
bool parseScheme(const std::string& name, SurfScheme& out);
bool schemeNeedsTriangles(SurfScheme s);

// One refinement step. Loop/Butterfly assume triangles (subdivide() triangulates).
Mesh catmullClarkStep(const Mesh& m);
Mesh loopStep(const Mesh& m);
Mesh dooSabinStep(const Mesh& m);
Mesh butterflyStep(const Mesh& m);

struct SubdivResult {
    Mesh mesh;
    int levelsApplied = 0;
    bool cappedByBudget = false;
    bool triangulatedFirst = false;
    double milliseconds = 0.0;
    // Face count after every level, [0] = the control cage.
    std::vector<int> faceHistory;
    std::vector<int> vertHistory;
};

// Apply `levels` steps of `scheme`, stopping early if the next step would push
// the face count past `faceBudget` (subdivision is exponential; the lecture's
// warning about freezing the machine is a real one).
SubdivResult subdivide(const Mesh& cage, SurfScheme scheme, int levels, int faceBudget = 400000);

}  // namespace sl
