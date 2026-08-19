// main.cpp — entry point.
//
//   subdivlab                       interactive window
//   subdivlab --shot out.png ...    render one frame offscreen
//   subdivlab --gallery DIR         render the documentation screenshot set
//   subdivlab --selftest            numeric verification of the schemes
//
// The interactive and offscreen paths share renderFrame(), so a saved PNG is
// exactly what the window shows.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "app.h"
#include "png.h"
#include "x11window.h"

using namespace sl;

namespace {

struct Options {
    std::string shot;
    std::string galleryDir;
    bool selftest = false;
    bool listOptions = false;
    int width = 1440, height = 880;
    int ss = 2;
    bool ssSet = false;
    bool curveParamSet = false;
    AppState st;
};

void printUsage() {
    std::printf(
        "Subdivision Lab 3D — surface subdivision in a from-scratch software renderer\n"
        "\n"
        "  subdivlab                        open the interactive window\n"
        "  subdivlab --shot FILE.png        render one frame to a PNG and exit\n"
        "  subdivlab --gallery DIR          render every documentation screenshot\n"
        "  subdivlab --selftest             verify the schemes numerically\n"
        "\n"
        "Scene:\n"
        "  --mode surface|curve|terrain|compare|levels\n"
        "  --mesh cube|tetrahedron|octahedron|icosahedron|torus|plane|cylinder|\n"
        "         lblock|cross|pyramid\n"
        "  --scheme catmull-clark|loop|doo-sabin|butterfly|none\n"
        "  --level N                        subdivision level (0..5)\n"
        "  --curve-scheme chaikin|four-point|midpoint\n"
        "  --curve-preset N   --curve-param F   --curve-level N\n"
        "  --roughness F      --seed N\n"
        "\n"
        "View:\n"
        "  --width N  --height N  --ss N    canvas size and supersampling (1..4)\n"
        "  --yaw F  --pitch F  --dist F     camera, radians / world units\n"
        "  --shading flat|gouraud|phong\n"
        "  --no-cage  --no-wire  --no-grid  --no-hud\n"
        "  --extraordinary  --normals\n"
        "\n"
        "Interactive keys:\n"
        "  1-5 mode   S/shift-S scheme   M/shift-M cage   [ ] level   , . parameter\n"
        "  F shading  W wireframe  C cage  G grid  X extraordinary  N normals\n"
        "  A auto-spin  R reset view  E re-roll seed  H hide HUD  P save PNG  Q quit\n"
        "  drag / arrows orbit, wheel or z/Z zoom\n");
}

bool parseArgs(int argc, char** argv, Options& o) {
    auto need = [&](int& i) -> const char* {
        if (i + 1 >= argc) {
            std::fprintf(stderr, "error: %s needs a value\n", argv[i]);
            std::exit(2);
        }
        return argv[++i];
    };

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { printUsage(); std::exit(0); }
        else if (a == "--shot")     o.shot = need(i);
        else if (a == "--gallery")  o.galleryDir = need(i);
        else if (a == "--selftest") o.selftest = true;
        else if (a == "--width")    o.width = std::atoi(need(i));
        else if (a == "--height")   o.height = std::atoi(need(i));
        else if (a == "--ss")       { o.ss = std::atoi(need(i)); o.ssSet = true; }
        else if (a == "--level")    o.st.level = std::atoi(need(i));
        else if (a == "--curve-level")  o.st.curveLevel = std::atoi(need(i));
        else if (a == "--curve-preset") o.st.curvePreset = std::atoi(need(i));
        else if (a == "--curve-param")  { o.st.curveParam = std::atof(need(i)); o.curveParamSet = true; }
        else if (a == "--roughness")    o.st.terrain.roughness = std::atof(need(i));
        else if (a == "--seed")         { o.st.terrain.seed = uint32_t(std::strtoul(need(i), nullptr, 10)); o.st.curveSeed = o.st.terrain.seed; }
        else if (a == "--terrain-level") o.st.terrain.levels = std::atoi(need(i));
        else if (a == "--yaw")      o.st.cam.yaw = std::atof(need(i));
        else if (a == "--pitch")    o.st.cam.pitch = std::atof(need(i));
        else if (a == "--dist")     o.st.cam.distance = std::atof(need(i));
        else if (a == "--no-cage")  o.st.showCage = false;
        else if (a == "--no-wire")  o.st.showWire = false;
        else if (a == "--no-grid")  o.st.showGrid = false;
        else if (a == "--no-hud")   o.st.showHud = false;
        else if (a == "--extraordinary") o.st.showExtraordinary = true;
        else if (a == "--normals")  o.st.showNormals = true;
        else if (a == "--mode") {
            if (!parseMode(need(i), o.st.mode)) { std::fprintf(stderr, "error: unknown mode\n"); return false; }
        } else if (a == "--mesh") {
            if (!parseBaseMesh(need(i), o.st.base)) { std::fprintf(stderr, "error: unknown mesh\n"); return false; }
        } else if (a == "--scheme") {
            if (!parseScheme(need(i), o.st.scheme)) { std::fprintf(stderr, "error: unknown scheme\n"); return false; }
        } else if (a == "--curve-scheme") {
            if (!parseCurveScheme(need(i), o.st.curveScheme)) { std::fprintf(stderr, "error: unknown curve scheme\n"); return false; }
        } else if (a == "--shading") {
            std::string s = need(i);
            if (s == "flat") o.st.shading = Shading::Flat;
            else if (s == "gouraud") o.st.shading = Shading::Gouraud;
            else if (s == "phong") o.st.shading = Shading::Phong;
            else { std::fprintf(stderr, "error: unknown shading\n"); return false; }
        } else {
            std::fprintf(stderr, "error: unknown option %s (try --help)\n", a.c_str());
            return false;
        }
    }
    // Each curve scheme has its own natural parameter; only override the
    // default when the caller actually asked for a value.
    if (!o.curveParamSet) o.st.curveParam = curveParamDefault(o.st.curveScheme);
    return true;
}

bool saveCanvas(const Canvas& cv, const std::string& path) {
    return writePNG(path, cv.w, cv.h, cv.toRGB());
}

int runShot(Options& o) {
    o.st.recompute();
    Canvas cv;
    FrameInfo fi = renderFrame(o.st, cv, o.width, o.height, o.ss);
    if (!saveCanvas(cv, o.shot)) {
        std::fprintf(stderr, "error: cannot write %s\n", o.shot.c_str());
        return 1;
    }
    std::printf("%s  %dx%d ss%d  %s  %lld tris  %.0f ms\n", o.shot.c_str(), o.width, o.height,
                o.ss, statusLine(o.st).c_str(), fi.triangles, fi.renderMs);
    return 0;
}

// --------------------------------------------------------------- gallery

struct Shot {
    const char* file;
    const char* args;
};

int runGallery(const std::string& dir, int argc, char** argv) {
    // Each entry re-invokes the same binary so the recipes double as
    // documentation: every screenshot in the README can be reproduced verbatim.
    static const Shot shots[] = {
        {"01-catmull-clark-cube.png",
         "--mode surface --mesh cube --scheme catmull-clark --level 3 --yaw 0.68 --pitch 0.38"},
        {"02-level-progression.png",
         "--mode levels --mesh cube --scheme catmull-clark --yaw 0.68 --pitch 0.38"},
        {"03-four-schemes-compared.png",
         "--mode compare --mesh cube --level 2 --yaw 0.72 --pitch 0.36"},
        {"04-butterfly-interpolating.png",
         "--mode surface --mesh octahedron --scheme butterfly --level 3 --yaw 0.9 --pitch 0.30"},
        {"05-loop-icosahedron.png",
         "--mode surface --mesh icosahedron --scheme loop --level 3 --yaw 0.5 --pitch 0.30"},
        {"06-doo-sabin-dual.png",
         "--mode surface --mesh lblock --scheme doo-sabin --level 3 --yaw 1.05 --pitch 0.42"},
        {"07-extraordinary-vertices.png",
         "--mode surface --mesh cross --scheme catmull-clark --level 3 --extraordinary "
         "--no-cage --yaw 0.85 --pitch 0.45"},
        {"08-boundary-rules.png",
         "--mode surface --mesh plane --scheme catmull-clark --level 3 --yaw 0.75 --pitch 0.55 "
         "--dist 4.2"},
        {"09-flat-shading.png",
         "--mode surface --mesh torus --scheme catmull-clark --level 2 --shading flat "
         "--no-cage --no-wire --yaw 0.7 --pitch 0.62"},
        {"10-phong-shading.png",
         "--mode surface --mesh torus --scheme catmull-clark --level 2 --shading phong "
         "--no-cage --no-wire --yaw 0.7 --pitch 0.62"},
        {"11-curve-four-point.png",
         "--mode curve --curve-scheme four-point --curve-preset 0 --curve-level 6 "
         "--yaw 0.5 --pitch 0.85 --dist 3.6"},
        {"12-curve-fractal-weight.png",
         "--mode curve --curve-scheme four-point --curve-preset 0 --curve-level 6 "
         "--curve-param 0.25 --yaw 0.5 --pitch 0.85 --dist 3.6"},
        {"13-terrain-diamond-square.png",
         "--mode terrain --terrain-level 7 --roughness 0.55 --seed 20260819 "
         "--yaw 0.9 --pitch 0.42"},
        {"14-torus-genus-one.png",
         "--mode surface --mesh torus --scheme catmull-clark --level 3 "
         "--yaw 0.62 --pitch 0.72 --dist 5.0"},
        {"15-open-cylinder-loop.png",
         "--mode surface --mesh cylinder --scheme loop --level 3 "
         "--yaw 0.75 --pitch 0.30"},
        {"16-curve-chaikin-open.png",
         "--mode curve --curve-scheme chaikin --curve-preset 2 --curve-level 5 "
         "--yaw 0.9 --pitch 0.18 --dist 4.2"},
    };

    for (const Shot& s : shots) {
        Options o;
        o.width = 1440;
        o.height = 880;
        o.ss = 3;
        std::vector<std::string> toks;
        toks.push_back(argv[0]);
        std::string cur;
        for (const char* p = s.args;; p++) {
            if (*p == ' ' || *p == 0) {
                if (!cur.empty()) { toks.push_back(cur); cur.clear(); }
                if (*p == 0) break;
            } else {
                cur += *p;
            }
        }
        std::vector<char*> cargv;
        for (std::string& t : toks) cargv.push_back(const_cast<char*>(t.c_str()));
        if (!parseArgs(int(cargv.size()), cargv.data(), o)) return 1;

        o.shot = dir + "/" + s.file;
        if (runShot(o) != 0) return 1;
    }
    (void)argc;
    std::printf("\ngallery written to %s\n", dir.c_str());
    return 0;
}

// -------------------------------------------------------------- selftest

int failures = 0;
void check(bool ok, const std::string& what) {
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) failures++;
}

int runSelftest() {
    std::printf("Subdivision Lab — self test\n\n");

    std::printf("Topology: Euler characteristic V-E+F is preserved by every scheme\n");
    for (int b = 0; b < int(BaseMesh::Count); b++) {
        Mesh cage = makeBaseMesh(BaseMesh(b));
        for (int s = 1; s < int(SurfScheme::Count); s++) {
            SurfScheme sc = SurfScheme(s);
            Mesh start = schemeNeedsTriangles(sc) ? triangulateMesh(cage) : cage;
            Topology t0 = buildTopology(start);
            int chi0 = computeStats(start, t0).euler;
            SubdivResult r = subdivide(cage, sc, 3);
            Topology t = buildTopology(r.mesh);
            MeshStats st = computeStats(r.mesh, t);
            if (st.euler != chi0 || t.hasNonManifold) {
                check(false, std::string(baseMeshName(BaseMesh(b))) + " / " + schemeName(sc));
            }
        }
    }
    check(failures == 0, "40 mesh/scheme combinations keep their Euler characteristic");

    std::printf("\nKnown refinement counts on the cube\n");
    Mesh cube = makeBaseMesh(BaseMesh::Cube);
    {
        SubdivResult r = subdivide(cube, SurfScheme::CatmullClark, 1);
        Topology t = buildTopology(r.mesh);
        MeshStats s = computeStats(r.mesh, t);
        check(s.verts == 26 && s.edges == 48 && s.faces == 24,
              "Catmull-Clark level 1: 26 V / 48 E / 24 F  (V+E+F of the cage)");
    }
    {
        SubdivResult r = subdivide(cube, SurfScheme::DooSabin, 1);
        Topology t = buildTopology(r.mesh);
        MeshStats s = computeStats(r.mesh, t);
        check(s.verts == 24 && s.edges == 48 && s.faces == 26,
              "Doo-Sabin level 1: 24 V / 48 E / 26 F  (exactly the dual)");
        bool allFour = s.minValence == 4 && s.maxValence == 4;
        check(allFour, "Doo-Sabin makes every vertex valence 4");
    }

    std::printf("\nInterpolating vs approximating\n");
    {
        double worst = 0;
        for (int b = 0; b < int(BaseMesh::Count); b++) {
            Mesh m = triangulateMesh(makeBaseMesh(BaseMesh(b)));
            SubdivResult r = subdivide(m, SurfScheme::Butterfly, 3);
            for (int i = 0; i < m.numVerts(); i++)
                worst = std::max(worst, length(r.mesh.V[i] - m.V[i]));
        }
        check(worst < 1e-12, "Butterfly leaves every control vertex exactly in place");

        SubdivResult r = subdivide(cube, SurfScheme::CatmullClark, 1);
        check(length(r.mesh.V[0] - cube.V[0]) > 0.1,
              "Catmull-Clark pulls control vertices inward (approximating)");
    }

    std::printf("\nBoundary rules\n");
    {
        Mesh plane = makeBaseMesh(BaseMesh::Plane);
        Topology t = buildTopology(plane);
        int corner = -1;
        for (int v = 0; v < plane.numVerts(); v++)
            if (t.valence(v) == 2) { corner = v; break; }
        SubdivResult r = subdivide(plane, SurfScheme::CatmullClark, 3);
        check(corner >= 0 && length(r.mesh.V[corner] - plane.V[corner]) < 1e-12,
              "Catmull-Clark pins the corners of an open patch");
        SubdivResult rl = subdivide(plane, SurfScheme::Loop, 3);
        check(!buildTopology(rl.mesh).hasNonManifold, "Loop keeps an open patch manifold");
    }

    std::printf("\nCurve schemes\n");
    {
        std::vector<Vec3> square = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
        std::vector<Vec3> c = chaikinStep(square, true, 0.25);
        check(c.size() == 8, "Chaikin on a square yields 8 points");
        bool ok = std::fabs(c[0].x - 0.25) < 1e-12 && std::fabs(c[1].x - 0.75) < 1e-12;
        check(ok, "Chaikin cuts at 1/4 and 3/4 (the classic 1:3 cut)");

        std::vector<Vec3> f = fourPointStep(square, true, 0.0);
        check(std::fabs(f[1].x - 0.5) < 1e-12 && std::fabs(f[1].y - 0.0) < 1e-12,
              "Four-Point with w = 0 gives plain midpoints");
        std::vector<Vec3> g = fourPointStep(square, true, 1.0 / 16);
        bool keeps = length(g[0] - square[0]) < 1e-15 && length(g[2] - square[1]) < 1e-15;
        check(keeps, "Four-Point keeps the control points (interpolating)");
    }

    std::printf("\nBudgets\n");
    {
        SubdivResult r = subdivide(makeBaseMesh(BaseMesh::Torus), SurfScheme::CatmullClark, 9);
        check(r.cappedByBudget && r.mesh.numFaces() <= 400000,
              "the face budget stops runaway subdivision");
    }

    std::printf("\nRenderer\n");
    {
        Canvas cv;
        AppState st;
        st.recompute();
        FrameInfo fi = renderFrame(st, cv, 400, 260, 1);
        bool nonEmpty = false;
        for (uint32_t p : cv.px)
            if (p != cv.px[0]) { nonEmpty = true; break; }
        check(cv.w == 400 && cv.h == 260 && nonEmpty && fi.triangles > 0,
              "renderFrame produces a non-trivial image");
        std::vector<uint8_t> rgbv = cv.toRGB();
        check(rgbv.size() == 400u * 260u * 3u, "canvas converts to a packed RGB buffer");
    }

    std::printf("\n%s (%d failure%s)\n", failures ? "SELF TEST FAILED" : "ALL CHECKS PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}

// ----------------------------------------------------------- interactive

int runInteractive(Options& o) {
    X11Window win;
    if (!win.open(o.width, o.height, "Subdivision Lab 3D")) {
        std::fprintf(stderr, "error: %s\n", win.error());
        std::fprintf(stderr, "hint: subdivlab --shot out.png  renders without a display\n");
        return 1;
    }

    o.st.resetCamera();
    o.st.recompute();

    Canvas cv;
    bool dirty = true;
    bool dragging = false;
    int lastX = 0, lastY = 0;
    int shotCounter = 0;
    int w = o.width, h = o.height;
    const int ss = o.ssSet ? o.ss : 1;   // interactive defaults to no supersampling

    for (;;) {
        WindowEvent ev;
        bool quit = false;
        while (win.poll(ev)) {
            switch (ev.type) {
                case WindowEvent::Close: quit = true; break;
                case WindowEvent::Resize:
                    w = win.width();
                    h = win.height();
                    dirty = true;
                    break;
                case WindowEvent::MouseDown:
                    if (ev.button == 1) { dragging = true; lastX = ev.x; lastY = ev.y; }
                    break;
                case WindowEvent::MouseUp:
                    if (ev.button == 1) dragging = false;
                    break;
                case WindowEvent::MouseMove:
                    if (dragging) {
                        o.st.cam.orbit((ev.x - lastX) * 0.008, (ev.y - lastY) * 0.008);
                        lastX = ev.x;
                        lastY = ev.y;
                        dirty = true;
                    }
                    break;
                case WindowEvent::Wheel:
                    o.st.cam.zoom(ev.dir > 0 ? 0.90 : 1.11);
                    dirty = true;
                    break;
                case WindowEvent::Key: {
                    if (ev.key == 'p' || ev.key == 'P') {
                        char name[64];
                        std::snprintf(name, sizeof name, "subdivlab-%03d.png", ++shotCounter);
                        Canvas hi;
                        renderFrame(o.st, hi, 1440, 880, 3);
                        if (saveCanvas(hi, name)) std::printf("saved %s\n", name);
                        break;
                    }
                    bool q = false;
                    if (handleKey(o.st, ev.key, q)) dirty = true;
                    if (q) quit = true;
                    break;
                }
                default: break;
            }
        }
        if (quit) break;

        if (o.st.autoRotate) {
            o.st.cam.orbit(0.012, 0);
            dirty = true;
        }

        if (dirty) {
            renderFrame(o.st, cv, w, h, ss);
            win.present(cv);
            win.setTitle("Subdivision Lab 3D — " + statusLine(o.st));
            dirty = false;
        }
        win.waitEvent(o.st.autoRotate ? 16 : 200);
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Options o;
    if (!parseArgs(argc, argv, o)) return 2;

    if (o.selftest) return runSelftest();
    if (!o.galleryDir.empty()) return runGallery(o.galleryDir, argc, argv);
    if (!o.shot.empty()) return runShot(o);
    return runInteractive(o);
}
