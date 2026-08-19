// app.h — application state, scene composition and the on-screen HUD.
//
// One AppState drives everything. Interactive input and the offscreen
// screenshot path both go through renderFrame(), so a saved PNG is pixel-wise
// the same image the window shows.
#pragma once

#include <string>
#include <vector>

#include "canvas.h"
#include "mesh.h"
#include "render.h"
#include "subdiv_curve.h"
#include "subdiv_surface.h"
#include "terrain.h"

namespace sl {

enum class Mode { Surface, Curve, Terrain, Compare, Count };
const char* modeName(Mode m);
bool parseMode(const std::string& s, Mode& out);

// A shared palette keeps the HUD and the 3D overlays consistent.
namespace theme {
const Vec3 bgTop      = rgb(0x191926);
const Vec3 bgBottom   = rgb(0x0D0D13);
const Vec3 panel      = rgb(0x1B1B27);
const Vec3 panelEdge  = rgb(0x2E2E40);
const Vec3 text       = rgb(0xE8E8F0);
const Vec3 textDim    = rgb(0x9A9AAE);
const Vec3 textFaint  = rgb(0x6C6C82);
const Vec3 accent     = rgb(0x4DD0A6);
const Vec3 accent2    = rgb(0x6AA9FF);
const Vec3 warn       = rgb(0xFFB454);
const Vec3 violet     = rgb(0xB98CFF);
const Vec3 cage       = rgb(0xFFB454);
const Vec3 cagePoint  = rgb(0xFFD79A);
const Vec3 wire       = rgb(0x1A2230);
const Vec3 grid       = rgb(0x2A2A3C);
}  // namespace theme

Vec3 schemeColor(SurfScheme s);

// The scheme's refinement rule, as three short lines for the HUD.
std::vector<std::string> schemeRule(SurfScheme s);

struct AppState {
    Mode mode = Mode::Surface;

    // ---- surface mode
    BaseMesh base = BaseMesh::Cube;
    SurfScheme scheme = SurfScheme::CatmullClark;
    int level = 2;

    // ---- curve mode
    int curvePreset = 0;
    CurveScheme curveScheme = CurveScheme::Chaikin;
    double curveParam = 0.25;
    int curveLevel = 4;
    uint32_t curveSeed = 20260819u;

    // ---- terrain mode
    TerrainParams terrain;

    // ---- view / display
    Camera cam;
    Shading shading = Shading::Phong;
    bool showCage = true;
    bool showWire = true;
    bool showGrid = true;
    bool showExtraordinary = false;
    bool showNormals = false;
    bool showHud = true;
    bool autoRotate = false;

    // ---- derived, filled by recompute()
    Mesh cage_;
    SubdivResult surf_;
    MeshStats cageStats_, surfStats_;
    std::vector<int> extraordinaryVerts_;
    std::vector<Vec3> control_;
    bool curveClosed_ = true;
    CurveResult curve_;
    TerrainResult terr_;
    SubdivResult cmp_[4];
    MeshStats cmpStats_[4];
    static const SurfScheme kCompareSchemes[4];

    void recompute();
    int maxLevel() const;
    double defaultCamDistance() const;
    void resetCamera();
};

struct FrameInfo {
    double renderMs = 0;
    long long triangles = 0;
    double maxEdgePx = 0;      // longest drawn edge, in resolved pixels
    int verts = 0, faces = 0;
};

FrameInfo renderFrame(const AppState& st, Canvas& out, int w, int h, int ss);

// Keyboard handling shared by the window and the CLI. Returns true if the
// state changed and needs a redraw; sets `quit` on Esc/Q.
bool handleKey(AppState& st, int ch, bool& quit);

// Human-readable description of the current configuration (window title, logs).
std::string statusLine(const AppState& st);

// --- HUD (hud.cpp)
void drawHud(Canvas& cv, const AppState& st, const FrameInfo& fi);

}  // namespace sl
