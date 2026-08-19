// render.h — the software 3D pipeline.
//
//   model -> world -> view -> clip -> (near-plane clip) -> NDC -> screen
//
// Triangles are rasterised with barycentric coverage, a z-buffer and
// perspective-correct attribute interpolation; lines and points go through the
// same depth buffer so wireframes and control cages occlude correctly.
// Everything is drawn into a supersampled target and box-filtered down, which
// is where the anti-aliasing comes from.
#pragma once

#include <cstdint>
#include <vector>

#include "canvas.h"
#include "mesh.h"
#include "vecmath.h"

namespace sl {

enum class Shading { Flat, Gouraud, Phong, Count };
const char* shadingName(Shading s);

struct Light {
    Vec3 keyDir   = normalize(Vec3(-0.45, 0.75, 0.55));
    Vec3 keyColor = Vec3(1.00, 0.97, 0.92);
    Vec3 fillDir  = normalize(Vec3(0.7, -0.25, -0.4));
    Vec3 fillColor = Vec3(0.24, 0.28, 0.40);
    Vec3 ambient  = Vec3(0.13, 0.14, 0.18);
    double specularStrength = 0.35;
    double shininess = 42.0;
    double rimStrength = 0.18;
    Vec3 rimColor = Vec3(0.45, 0.72, 1.0);
};

struct Material {
    Vec3 albedo = Vec3(0.55, 0.62, 0.72);
    Vec3 backAlbedo = Vec3(0.34, 0.30, 0.38);   // shown on back faces of open meshes
    bool useVertexColor = false;
    double alpha = 1.0;
};

struct Camera {
    Vec3 target{0, 0, 0};
    double distance = 4.7;
    double yaw = 0.65;      // radians, around +Y
    double pitch = 0.42;    // radians, above the XZ plane
    double fovDeg = 36.0;
    double nearZ = 0.05, farZ = 200.0;

    Vec3 eye() const;
    Vec3 up() const { return Vec3(0, 1, 0); }
    Mat4 view() const;
    Mat4 proj(double aspect) const;
    Mat4 viewProj(double aspect) const { return proj(aspect) * view(); }
    void orbit(double dyaw, double dpitch);
    void zoom(double factor);
};

// Supersampled colour + depth target.
struct RenderTarget {
    int w = 0, h = 0;          // final (resolved) size
    int ss = 1;                // supersample factor
    int sw = 0, sh = 0;        // internal size = w*ss, h*ss
    std::vector<uint32_t> color;
    std::vector<float> depth;

    void resize(int w_, int h_, int ss_);
    void clear(Vec3 top, Vec3 bottom);
    void resolveTo(Canvas& out) const;   // box-filter down to w x h

    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < sw && y < sh; }
};

struct DrawOptions {
    Shading shading = Shading::Phong;
    bool backfaceCull = true;
    bool twoSided = false;     // shade back faces with the flipped normal
    bool depthWrite = true;
    bool depthTest = true;
};

// A sub-rectangle of the target, in resolved (canvas) pixels. Comparison mode
// tiles several of these across one frame.
struct Viewport {
    int x = 0, y = 0, w = 0, h = 0;
    bool valid() const { return w > 0 && h > 0; }
};

class Renderer {
public:
    // `vp` defaults to the whole target. Geometry is scissored to it, and the
    // projection is remapped so the viewport behaves like its own screen.
    Renderer(RenderTarget& target, const Camera& cam, Viewport vp = Viewport{});

    void setLight(const Light& l) { light_ = l; }
    const Light& light() const { return light_; }
    Mat4 viewProj() const { return vp_; }

    // Filled geometry.
    void drawTriangles(const std::vector<RTri>& tris, const Material& mat, const DrawOptions& opt);
    void drawMesh(const Mesh& m, const Material& mat, const DrawOptions& opt);

    // Overlays. `bias` pulls the primitive toward the camera in NDC depth so it
    // survives the depth test against the surface it sits on.
    void drawLine3D(Vec3 a, Vec3 b, Vec3 color, double thickness, double bias = 2e-3,
                    double alpha = 1.0);
    void drawPoint3D(Vec3 p, Vec3 color, double radius, double bias = 4e-3, double alpha = 1.0);
    void drawWireframe(const Mesh& m, Vec3 color, double thickness, double bias = 2e-3,
                       double alpha = 1.0);
    void drawPolyline3D(const std::vector<Vec3>& pts, bool closed, Vec3 color, double thickness,
                        double bias = 2e-3, double alpha = 1.0);
    void drawGroundGrid(double extent, int divisions, double y, Vec3 color, double alpha);

    // Project a world point to resolved-canvas pixels; false if behind the eye.
    bool projectToCanvas(Vec3 p, double& outX, double& outY) const;

    long long trianglesDrawn() const { return triCount_; }

private:
    struct SVert {                 // post-clip vertex
        Vec4 clip;
        Vec3 world;
        Vec3 normal;
        Vec3 color;
    };

    void rasterTriangle(const SVert& a, const SVert& b, const SVert& c, const Material& mat,
                        const DrawOptions& opt, Vec3 faceNormal);
    Vec3 shade(Vec3 worldPos, Vec3 n, Vec3 albedo) const;

    RenderTarget& rt_;
    Camera cam_;
    Light light_;
    Mat4 vp_;
    Vec3 eye_;
    Viewport view_;
    int sx0_ = 0, sy0_ = 0, sx1_ = 0, sy1_ = 0;   // scissor, in supersampled pixels
    long long triCount_ = 0;
};

}  // namespace sl
