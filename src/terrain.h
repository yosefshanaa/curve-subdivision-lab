// terrain.h — diamond-square terrain: midpoint displacement generalised from a
// curve to a surface.
//
// The 2D prototype displaced the midpoint of every *edge*; here each square
// gets a displaced centre (diamond step) and each diamond gets a displaced
// edge midpoint (square step), with the displacement range halving each level.
// Same recipe, one dimension up — which is exactly the relationship between
// curve subdivision and surface subdivision.
#pragma once

#include <cstdint>
#include <vector>

#include "mesh.h"

namespace sl {

struct TerrainParams {
    int levels = 6;             // grid is (2^levels + 1) squared
    double roughness = 0.5;     // 0 = smooth rolling, 1 = jagged
    uint32_t seed = 12345;
    double heightScale = 0.55;
    double seaLevel = -0.20;    // heights below this are flattened into water
    bool colorByElevation = true;
};

struct TerrainResult {
    Mesh mesh;
    int gridSize = 0;
    double minHeight = 0, maxHeight = 0;
    double waterFraction = 0;
    double milliseconds = 0;
    std::vector<int> vertHistory;   // vertex count per level, for the stats panel
};

TerrainResult makeTerrain(const TerrainParams& p);

}  // namespace sl
