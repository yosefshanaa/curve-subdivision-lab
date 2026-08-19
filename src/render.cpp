#include "render.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace sl {

const char* shadingName(Shading s) {
    switch (s) {
        case Shading::Flat:    return "Flat";
        case Shading::Gouraud: return "Gouraud";
        case Shading::Phong:   return "Phong";
        default:               return "?";
    }
}

// ----------------------------------------------------------------- Camera

Vec3 Camera::eye() const {
    double cp = std::cos(pitch), sp = std::sin(pitch);
    return target + Vec3(distance * cp * std::sin(yaw), distance * sp, distance * cp * std::cos(yaw));
}

Mat4 Camera::view() const { return Mat4::lookAt(eye(), target, up()); }

Mat4 Camera::proj(double aspect) const {
    return Mat4::perspective(radians(fovDeg), aspect, nearZ, farZ);
}

void Camera::orbit(double dyaw, double dpitch) {
    yaw += dyaw;
    pitch = clampd(pitch + dpitch, -1.45, 1.45);   // stop short of the poles
}

void Camera::zoom(double factor) { distance = clampd(distance * factor, 0.6, 60.0); }

// ----------------------------------------------------------- RenderTarget

void RenderTarget::resize(int w_, int h_, int ss_) {
    w = std::max(1, w_);
    h = std::max(1, h_);
    ss = std::max(1, ss_);
    sw = w * ss;
    sh = h * ss;
    color.assign(size_t(sw) * sh, 0);
    depth.assign(size_t(sw) * sh, 1e30f);
}

void RenderTarget::clear(Vec3 top, Vec3 bottom) {
    for (int y = 0; y < sh; y++) {
        uint32_t v = packRGB(lerp(top, bottom, sh > 1 ? double(y) / (sh - 1) : 0.0));
        std::fill(color.begin() + size_t(y) * sw, color.begin() + size_t(y + 1) * sw, v);
    }
    std::fill(depth.begin(), depth.end(), 1e30f);
}

void RenderTarget::resolveTo(Canvas& out) const {
    out.resize(w, h);
    const double inv = 1.0 / (double(ss) * ss);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double r = 0, g = 0, b = 0;
            for (int j = 0; j < ss; j++) {
                const uint32_t* row = &color[size_t(y * ss + j) * sw + size_t(x * ss)];
                for (int i = 0; i < ss; i++) {
                    uint32_t p = row[i];
                    r += (p >> 16) & 0xFF;
                    g += (p >> 8) & 0xFF;
                    b += p & 0xFF;
                }
            }
            out.px[size_t(y) * w + x] = (uint32_t(r * inv + 0.5) << 16) |
                                        (uint32_t(g * inv + 0.5) << 8) |
                                         uint32_t(b * inv + 0.5);
        }
    }
}

// -------------------------------------------------------------- Renderer

Renderer::Renderer(RenderTarget& target, const Camera& cam, Viewport vp)
    : rt_(target), cam_(cam), view_(vp) {
    if (!view_.valid()) view_ = Viewport{0, 0, rt_.w, rt_.h};

    // Remap the viewport's NDC cube onto the full target, so one frame can hold
    // several independent views.
    const double W = rt_.w, H = rt_.h;
    Mat4 remap = Mat4::identity();
    remap.m[0][0] = view_.w / W;
    remap.m[0][3] = (2.0 * view_.x + view_.w) / W - 1.0;
    remap.m[1][1] = view_.h / H;
    remap.m[1][3] = 1.0 - (2.0 * view_.y + view_.h) / H;

    vp_ = remap * cam_.viewProj(double(view_.w) / std::max(1, view_.h));
    eye_ = cam_.eye();

    sx0_ = std::max(0, view_.x * rt_.ss);
    sy0_ = std::max(0, view_.y * rt_.ss);
    sx1_ = std::min(rt_.sw - 1, (view_.x + view_.w) * rt_.ss - 1);
    sy1_ = std::min(rt_.sh - 1, (view_.y + view_.h) * rt_.ss - 1);
}

Vec3 Renderer::shade(Vec3 worldPos, Vec3 n, Vec3 albedo) const {
    Vec3 V = normalize(eye_ - worldPos);

    // Key + fill diffuse.
    double nl = std::max(0.0, dot(n, light_.keyDir));
    double nf = std::max(0.0, dot(n, light_.fillDir));
    Vec3 diffuse = mul(albedo, light_.keyColor * nl + light_.fillColor * nf);

    // Blinn-Phong specular from the key light only.
    Vec3 H = normalize(light_.keyDir + V);
    double spec = std::pow(std::max(0.0, dot(n, H)), light_.shininess) * light_.specularStrength;
    if (nl <= 0.0) spec = 0.0;

    // A touch of rim light to separate the silhouette from the background.
    double rim = std::pow(1.0 - std::min(1.0, std::max(0.0, dot(n, V))), 3.0) * light_.rimStrength;

    return mul(albedo, light_.ambient) + diffuse + light_.keyColor * spec + light_.rimColor * rim;
}

void Renderer::rasterTriangle(const SVert& a, const SVert& b, const SVert& c, const Material& mat,
                              const DrawOptions& opt, Vec3 faceNormal) {
    // Perspective divide + viewport transform.
    const SVert* v[3] = {&a, &b, &c};
    double sx[3], sy[3], sz[3], invW[3];
    for (int i = 0; i < 3; i++) {
        double iw = 1.0 / v[i]->clip.w;
        invW[i] = iw;
        sx[i] = (v[i]->clip.x * iw * 0.5 + 0.5) * rt_.sw;
        sy[i] = (0.5 - v[i]->clip.y * iw * 0.5) * rt_.sh;
        sz[i] = v[i]->clip.z * iw;
    }

    double area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
    if (std::fabs(area) < 1e-12) return;
    // Screen y points down, so a front face (CCW in NDC) has negative area here.
    bool back = area > 0.0;
    if (back && opt.backfaceCull && !opt.twoSided) return;

    Vec3 albedo = back && opt.twoSided ? mat.backAlbedo : mat.albedo;
    Vec3 nFlip = back ? -faceNormal : faceNormal;

    int x0 = std::max(sx0_, int(std::floor(std::min({sx[0], sx[1], sx[2]}))));
    int x1 = std::min(sx1_, int(std::ceil(std::max({sx[0], sx[1], sx[2]}))));
    int y0 = std::max(sy0_, int(std::floor(std::min({sy[0], sy[1], sy[2]}))));
    int y1 = std::min(sy1_, int(std::ceil(std::max({sy[0], sy[1], sy[2]}))));
    if (x0 > x1 || y0 > y1) return;

    // Flat shading evaluates the lighting once, at the face centroid.
    Vec3 flatColor(0, 0, 0);
    if (opt.shading == Shading::Flat) {
        Vec3 centre = (a.world + b.world + c.world) / 3.0;
        Vec3 base = mat.useVertexColor ? (a.color + b.color + c.color) / 3.0 : albedo;
        flatColor = shade(centre, nFlip, base);
    }
    // Gouraud evaluates it at the three corners and interpolates the result.
    Vec3 gour[3];
    if (opt.shading == Shading::Gouraud) {
        for (int i = 0; i < 3; i++) {
            Vec3 n = back ? -v[i]->normal : v[i]->normal;
            gour[i] = shade(v[i]->world, normalize(n), mat.useVertexColor ? v[i]->color : albedo);
        }
    }

    const double invArea = 1.0 / area;
    for (int y = y0; y <= y1; y++) {
        double py = y + 0.5;
        for (int x = x0; x <= x1; x++) {
            double px = x + 0.5;
            double w0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) * invArea;
            double w1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) * invArea;
            double w2 = 1.0 - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;

            double z = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
            size_t idx = size_t(y) * rt_.sw + x;
            if (opt.depthTest && z >= rt_.depth[idx]) continue;

            Vec3 out;
            if (opt.shading == Shading::Flat) {
                out = flatColor;
            } else if (opt.shading == Shading::Gouraud) {
                out = gour[0] * w0 + gour[1] * w1 + gour[2] * w2;
            } else {
                // Phong: perspective-correct interpolation of position + normal.
                double iw = w0 * invW[0] + w1 * invW[1] + w2 * invW[2];
                double c0 = w0 * invW[0] / iw, c1 = w1 * invW[1] / iw, c2 = w2 * invW[2] / iw;
                Vec3 wp = a.world * c0 + b.world * c1 + c.world * c2;
                Vec3 n = normalize(a.normal * c0 + b.normal * c1 + c.normal * c2);
                if (back) n = -n;
                Vec3 base = mat.useVertexColor ? (a.color * c0 + b.color * c1 + c.color * c2) : albedo;
                out = shade(wp, n, base);
            }

            if (mat.alpha < 1.0) out = lerp(unpackRGB(rt_.color[idx]), out, mat.alpha);
            rt_.color[idx] = packRGB(out);
            if (opt.depthWrite) rt_.depth[idx] = float(z);
        }
    }
    triCount_++;
}

void Renderer::drawTriangles(const std::vector<RTri>& tris, const Material& mat,
                             const DrawOptions& opt) {
    const double kNear = 1e-5;
    std::vector<SVert> poly, clipped;
    poly.reserve(4);
    clipped.reserve(5);

    for (const RTri& t : tris) {
        poly.clear();
        for (int i = 0; i < 3; i++) {
            SVert s;
            s.world = t.p[i];
            s.normal = t.n[i];
            s.color = t.c[i];
            s.clip = vp_ * Vec4(t.p[i], 1.0);
            poly.push_back(s);
        }
        Vec3 fn = normalize(cross(t.p[1] - t.p[0], t.p[2] - t.p[0]));
        if (lengthSq(fn) < 1e-18) continue;

        // Sutherland-Hodgman against the single plane w > kNear. Only the near
        // plane needs clipping; the rest is handled by screen-space bounds.
        bool needsClip = false;
        for (const SVert& s : poly) if (s.clip.w <= kNear) needsClip = true;
        if (!needsClip) {
            rasterTriangle(poly[0], poly[1], poly[2], mat, opt, fn);
            continue;
        }
        clipped.clear();
        for (size_t i = 0; i < poly.size(); i++) {
            const SVert& cur = poly[i];
            const SVert& nxt = poly[(i + 1) % poly.size()];
            bool inCur = cur.clip.w > kNear, inNxt = nxt.clip.w > kNear;
            if (inCur) clipped.push_back(cur);
            if (inCur != inNxt) {
                double tt = (kNear - cur.clip.w) / (nxt.clip.w - cur.clip.w);
                SVert m;
                m.clip = lerp(cur.clip, nxt.clip, tt);
                m.world = lerp(cur.world, nxt.world, tt);
                m.normal = normalize(lerp(cur.normal, nxt.normal, tt));
                m.color = lerp(cur.color, nxt.color, tt);
                clipped.push_back(m);
            }
        }
        for (size_t i = 1; i + 1 < clipped.size(); i++)
            rasterTriangle(clipped[0], clipped[i], clipped[i + 1], mat, opt, fn);
    }
}

void Renderer::drawMesh(const Mesh& m, const Material& mat, const DrawOptions& opt) {
    Material use = mat;
    if (use.useVertexColor && m.vertexColor.size() != m.V.size()) use.useVertexColor = false;
    drawTriangles(triangulate(m, opt.shading != Shading::Flat), use, opt);
}

void Renderer::drawLine3D(Vec3 a, Vec3 b, Vec3 color, double thickness, double bias,
                          double alpha) {
    const double kNear = 1e-5;
    Vec4 ca = vp_ * Vec4(a, 1.0), cb = vp_ * Vec4(b, 1.0);
    if (ca.w <= kNear && cb.w <= kNear) return;
    if (ca.w <= kNear) {
        double t = (kNear - ca.w) / (cb.w - ca.w);
        ca = lerp(ca, cb, t);
    } else if (cb.w <= kNear) {
        double t = (kNear - cb.w) / (ca.w - cb.w);
        cb = lerp(cb, ca, t);
    }

    double iwa = 1.0 / ca.w, iwb = 1.0 / cb.w;
    double ax = (ca.x * iwa * 0.5 + 0.5) * rt_.sw, ay = (0.5 - ca.y * iwa * 0.5) * rt_.sh;
    double bx = (cb.x * iwb * 0.5 + 0.5) * rt_.sw, by = (0.5 - cb.y * iwb * 0.5) * rt_.sh;
    double az = ca.z * iwa - bias, bz = cb.z * iwb - bias;

    double dx = bx - ax, dy = by - ay;
    double len = std::sqrt(dx * dx + dy * dy);
    double half = std::max(0.5, thickness * 0.5);

    int x0 = std::max(sx0_, int(std::floor(std::min(ax, bx) - half - 1)));
    int x1 = std::min(sx1_, int(std::ceil(std::max(ax, bx) + half + 1)));
    int y0 = std::max(sy0_, int(std::floor(std::min(ay, by) - half - 1)));
    int y1 = std::min(sy1_, int(std::ceil(std::max(ay, by) + half + 1)));
    if (x0 > x1 || y0 > y1) return;

    double ux = len > 1e-9 ? dx / len : 0.0, uy = len > 1e-9 ? dy / len : 0.0;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            double rx = x + 0.5 - ax, ry = y + 0.5 - ay;
            double proj = len > 1e-9 ? clampd(rx * ux + ry * uy, 0.0, len) : 0.0;
            double ox = rx - ux * proj, oy = ry - uy * proj;
            double d = std::sqrt(ox * ox + oy * oy);
            double cov = clampd(half + 0.5 - d, 0.0, 1.0) * alpha;
            if (cov <= 0) continue;
            double z = (len > 1e-9) ? lerpd(az, bz, proj / len) : az;
            size_t idx = size_t(y) * rt_.sw + x;
            if (z >= rt_.depth[idx]) continue;
            rt_.color[idx] = packRGB(lerp(unpackRGB(rt_.color[idx]), color, cov));
            if (cov > 0.5) rt_.depth[idx] = float(z);
        }
}

void Renderer::drawPoint3D(Vec3 p, Vec3 color, double radius, double bias, double alpha) {
    const double kNear = 1e-5;
    Vec4 c = vp_ * Vec4(p, 1.0);
    if (c.w <= kNear) return;
    double iw = 1.0 / c.w;
    double cx = (c.x * iw * 0.5 + 0.5) * rt_.sw, cy = (0.5 - c.y * iw * 0.5) * rt_.sh;
    double z = c.z * iw - bias;

    int x0 = std::max(sx0_, int(std::floor(cx - radius - 1))), x1 = std::min(sx1_, int(std::ceil(cx + radius + 1)));
    int y0 = std::max(sy0_, int(std::floor(cy - radius - 1))), y1 = std::min(sy1_, int(std::ceil(cy + radius + 1)));
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
            double cov = clampd(radius + 0.5 - std::sqrt(dx * dx + dy * dy), 0.0, 1.0) * alpha;
            if (cov <= 0) continue;
            size_t idx = size_t(y) * rt_.sw + x;
            if (z >= rt_.depth[idx]) continue;
            rt_.color[idx] = packRGB(lerp(unpackRGB(rt_.color[idx]), color, cov));
            if (cov > 0.5) rt_.depth[idx] = float(z);
        }
}

void Renderer::drawWireframe(const Mesh& m, Vec3 color, double thickness, double bias,
                             double alpha) {
    Topology t = buildTopology(m);
    for (const Topology::Edge& e : t.edges)
        drawLine3D(m.V[e.a], m.V[e.b], color, thickness, bias, alpha);
}

void Renderer::drawPolyline3D(const std::vector<Vec3>& pts, bool closed, Vec3 color,
                              double thickness, double bias, double alpha) {
    const int n = int(pts.size());
    if (n < 2) return;
    const int segs = closed ? n : n - 1;
    for (int i = 0; i < segs; i++)
        drawLine3D(pts[i], pts[(i + 1) % n], color, thickness, bias, alpha);
}

void Renderer::drawGroundGrid(double extent, int divisions, double y, Vec3 color, double alpha) {
    for (int i = 0; i <= divisions; i++) {
        double t = -extent + 2.0 * extent * i / divisions;
        bool axis = std::fabs(t) < 1e-9;
        double a = alpha * (axis ? 1.8 : 1.0);
        drawLine3D(Vec3(t, y, -extent), Vec3(t, y, extent), color, 1.0, 1e-4, std::min(1.0, a));
        drawLine3D(Vec3(-extent, y, t), Vec3(extent, y, t), color, 1.0, 1e-4, std::min(1.0, a));
    }
}

void Renderer::drawAxes(double len) {
    drawLine3D(Vec3(0, 0, 0), Vec3(len, 0, 0), Vec3(0.90, 0.36, 0.38), 1.6);
    drawLine3D(Vec3(0, 0, 0), Vec3(0, len, 0), Vec3(0.40, 0.82, 0.45), 1.6);
    drawLine3D(Vec3(0, 0, 0), Vec3(0, 0, len), Vec3(0.36, 0.60, 0.95), 1.6);
}

bool Renderer::projectToCanvas(Vec3 p, double& outX, double& outY) const {
    Vec4 c = vp_ * Vec4(p, 1.0);
    if (c.w <= 1e-5) return false;
    double iw = 1.0 / c.w;
    outX = (c.x * iw * 0.5 + 0.5) * rt_.w;
    outY = (0.5 - c.y * iw * 0.5) * rt_.h;
    (void)0;
    return true;
}

}  // namespace sl
