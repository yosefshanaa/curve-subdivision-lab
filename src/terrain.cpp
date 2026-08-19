#include "terrain.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace sl {
namespace {

inline uint32_t xorshift32(uint32_t& s) {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}
inline double rand11(uint32_t& s) { return (double(xorshift32(s)) / 2147483648.0) - 1.0; }

// Elevation ramp: water, shore, grass, rock, snow.
Vec3 elevationColor(double t, bool water) {
    if (water) {
        double d = clampd(-t * 3.0, 0.0, 1.0);   // deeper => darker
        return lerp(Vec3(0.18, 0.42, 0.58), Vec3(0.07, 0.16, 0.30), d);
    }
    struct Stop { double t; Vec3 c; };
    static const Stop stops[] = {
        {0.00, Vec3(0.78, 0.71, 0.52)},   // sand
        {0.10, Vec3(0.40, 0.53, 0.31)},   // grass
        {0.38, Vec3(0.27, 0.40, 0.24)},   // forest
        {0.62, Vec3(0.44, 0.42, 0.39)},   // rock
        {0.82, Vec3(0.62, 0.61, 0.60)},   // scree
        {1.00, Vec3(0.93, 0.94, 0.97)},   // snow
    };
    const int n = int(sizeof(stops) / sizeof(stops[0]));
    double k = clampd(t, 0.0, 1.0);
    for (int i = 0; i + 1 < n; i++) {
        if (k <= stops[i + 1].t) {
            double u = (k - stops[i].t) / (stops[i + 1].t - stops[i].t);
            return lerp(stops[i].c, stops[i + 1].c, clampd(u, 0.0, 1.0));
        }
    }
    return stops[n - 1].c;
}

}  // namespace

TerrainResult makeTerrain(const TerrainParams& p) {
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();

    TerrainResult r;
    const int levels = std::max(1, std::min(9, p.levels));
    const int N = (1 << levels) + 1;
    r.gridSize = N;

    std::vector<double> H(size_t(N) * N, 0.0);
    auto at = [&](int x, int z) -> double& { return H[size_t(z) * N + x]; };

    uint32_t rng = p.seed ? p.seed : 1u;
    // The four corners seed the recursion.
    at(0, 0)         = rand11(rng) * 0.35;
    at(N - 1, 0)     = rand11(rng) * 0.35;
    at(0, N - 1)     = rand11(rng) * 0.35;
    at(N - 1, N - 1) = rand11(rng) * 0.35;

    // Displacement range shrinks by `decay` per level. At decay = 0.5 it tracks
    // the halving grid spacing exactly (smooth); larger values leave more energy
    // in the fine levels, which is what "rough" means here.
    const double decay = 0.42 + 0.22 * clampd(p.roughness, 0.0, 1.0);

    double amp = 0.9;
    for (int step = N - 1; step > 1; step /= 2) {
        const int half = step / 2;

        // Diamond step: centre of each square.
        for (int z = 0; z + step < N; z += step)
            for (int x = 0; x + step < N; x += step) {
                double avg = (at(x, z) + at(x + step, z) + at(x, z + step) + at(x + step, z + step)) * 0.25;
                at(x + half, z + half) = avg + rand11(rng) * amp;
            }

        // Square step: midpoint of each diamond, skipping neighbours off-grid.
        for (int z = 0; z < N; z += half)
            for (int x = (z / half % 2 == 0) ? half : 0; x < N; x += step) {
                double sum = 0;
                int cnt = 0;
                if (x >= half)     { sum += at(x - half, z); cnt++; }
                if (x + half < N)  { sum += at(x + half, z); cnt++; }
                if (z >= half)     { sum += at(x, z - half); cnt++; }
                if (z + half < N)  { sum += at(x, z + half); cnt++; }
                if (cnt) at(x, z) = sum / cnt + rand11(rng) * amp;
            }

        amp *= decay;   // the displacement range shrinks each level
        int side = (N - 1) / half + 1;
        r.vertHistory.push_back(side * side);
    }

    double lo = H[0], hi = H[0];
    for (double v : H) { lo = std::min(lo, v); hi = std::max(hi, v); }
    const double span = std::max(1e-9, hi - lo);
    r.minHeight = lo;
    r.maxHeight = hi;

    // Build the quad grid.
    Mesh& m = r.mesh;
    m.V.resize(size_t(N) * N);
    if (p.colorByElevation) m.vertexColor.resize(size_t(N) * N);

    int waterVerts = 0;
    for (int z = 0; z < N; z++)
        for (int x = 0; x < N; x++) {
            double t = (at(x, z) - lo) / span;             // 0..1
            double y = t * 2.0 - 1.0;                      // -1..1
            bool water = y < p.seaLevel;
            if (water) { y = p.seaLevel; waterVerts++; }
            double u = -1.0 + 2.0 * x / (N - 1);
            double v = -1.0 + 2.0 * z / (N - 1);
            size_t idx = size_t(z) * N + x;
            m.V[idx] = Vec3(u, y * p.heightScale, v);
            if (p.colorByElevation) {
                double above = (y - p.seaLevel) / std::max(1e-6, 1.0 - p.seaLevel);
                m.vertexColor[idx] = elevationColor(water ? y : above, water);
            }
        }
    r.waterFraction = double(waterVerts) / double(N) / double(N);

    m.F.reserve(size_t(N - 1) * (N - 1));
    for (int z = 0; z + 1 < N; z++)
        for (int x = 0; x + 1 < N; x++) {
            int a = z * N + x, b = z * N + x + 1, c = (z + 1) * N + x + 1, d = (z + 1) * N + x;
            m.F.push_back({a, d, c, b});   // CCW seen from +Y
        }

    m.computeNormals();
    r.milliseconds = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
    return r;
}

}  // namespace sl
