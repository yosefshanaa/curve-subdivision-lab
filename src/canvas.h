// canvas.h — 2D drawing surface: the final RGB image plus the primitives the
// HUD is built from (rectangles, anti-aliased lines, circles, text).
//
// The 3D scene is rasterised into a supersampled Framebuffer and resolved into
// a Canvas; the HUD is then drawn at native resolution so text stays crisp.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "vecmath.h"

namespace sl {

inline uint32_t packRGB(Vec3 c) {
    Vec3 k = clamp01(c);
    return (uint32_t(k.x * 255.0 + 0.5) << 16) | (uint32_t(k.y * 255.0 + 0.5) << 8) |
            uint32_t(k.z * 255.0 + 0.5);
}
inline Vec3 unpackRGB(uint32_t p) {
    return Vec3(((p >> 16) & 0xFF) / 255.0, ((p >> 8) & 0xFF) / 255.0, (p & 0xFF) / 255.0);
}
// #RRGGBB literal, e.g. rgb(0x4DD0A6).
inline Vec3 rgb(uint32_t hex) { return unpackRGB(hex); }

struct Canvas {
    int w = 0, h = 0;
    std::vector<uint32_t> px;

    void resize(int w_, int h_);
    void fill(Vec3 c);
    void verticalGradient(Vec3 top, Vec3 bottom);

    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    uint32_t get(int x, int y) const { return px[size_t(y) * w + x]; }
    void set(int x, int y, uint32_t v) { px[size_t(y) * w + x] = v; }

    void blend(int x, int y, Vec3 c, double a);

    void rect(int x, int y, int rw, int rh, Vec3 c, double a = 1.0);
    void rectOutline(int x, int y, int rw, int rh, Vec3 c, double a = 1.0, int thickness = 1);
    void roundRect(int x, int y, int rw, int rh, int radius, Vec3 c, double a = 1.0);
    void roundRectOutline(int x, int y, int rw, int rh, int radius, Vec3 c, double a = 1.0);
    void line(double x0, double y0, double x1, double y1, Vec3 c, double a = 1.0,
              double thickness = 1.0);
    void circle(double cx, double cy, double r, Vec3 c, double a = 1.0);
    void circleOutline(double cx, double cy, double r, Vec3 c, double a = 1.0,
                       double thickness = 1.0);

    std::vector<uint8_t> toRGB() const;   // for the PNG writer
};

// ---------------------------------------------------------------- text

enum class Face { UI, UIBold, Title, Mono, MonoBold };

int   textWidth(Face f, const std::string& s);
int   textLineHeight(Face f);
int   textAscent(Face f);
// `x` is the left edge, `baselineY` the baseline.
void  drawText(Canvas& cv, Face f, int x, int baselineY, const std::string& s, Vec3 c,
               double a = 1.0);
// Convenience: draws with the top of the line box at `topY`.
void  drawTextTop(Canvas& cv, Face f, int x, int topY, const std::string& s, Vec3 c,
                  double a = 1.0);

}  // namespace sl
