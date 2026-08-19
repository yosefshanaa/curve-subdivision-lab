#include "canvas.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

#include "font_data.h"

namespace sl {

void Canvas::resize(int w_, int h_) {
    w = std::max(1, w_);
    h = std::max(1, h_);
    px.assign(size_t(w) * h, 0);
}

void Canvas::fill(Vec3 c) {
    std::fill(px.begin(), px.end(), packRGB(c));
}

void Canvas::verticalGradient(Vec3 top, Vec3 bottom) {
    for (int y = 0; y < h; y++) {
        uint32_t v = packRGB(lerp(top, bottom, h > 1 ? double(y) / (h - 1) : 0.0));
        std::fill(px.begin() + size_t(y) * w, px.begin() + size_t(y + 1) * w, v);
    }
}

void Canvas::blend(int x, int y, Vec3 c, double a) {
    if (a <= 0.0 || !inside(x, y)) return;
    if (a >= 1.0) { set(x, y, packRGB(c)); return; }
    set(x, y, packRGB(lerp(unpackRGB(get(x, y)), c, a)));
}

void Canvas::rect(int x, int y, int rw, int rh, Vec3 c, double a) {
    int x0 = std::max(0, x), y0 = std::max(0, y);
    int x1 = std::min(w, x + rw), y1 = std::min(h, y + rh);
    if (a >= 1.0) {
        uint32_t v = packRGB(c);
        for (int yy = y0; yy < y1; yy++)
            std::fill(px.begin() + size_t(yy) * w + x0, px.begin() + size_t(yy) * w + x1, v);
        return;
    }
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) blend(xx, yy, c, a);
}

void Canvas::rectOutline(int x, int y, int rw, int rh, Vec3 c, double a, int t) {
    rect(x, y, rw, t, c, a);
    rect(x, y + rh - t, rw, t, c, a);
    rect(x, y + t, t, rh - 2 * t, c, a);
    rect(x + rw - t, y + t, t, rh - 2 * t, c, a);
}

namespace {
// Signed distance to a rounded rectangle, used for anti-aliased panel corners.
double sdRoundRect(double px_, double py_, double hw, double hh, double r) {
    double qx = std::fabs(px_) - (hw - r);
    double qy = std::fabs(py_) - (hh - r);
    double ax = std::max(qx, 0.0), ay = std::max(qy, 0.0);
    return std::sqrt(ax * ax + ay * ay) + std::min(std::max(qx, qy), 0.0) - r;
}
}  // namespace

void Canvas::roundRect(int x, int y, int rw, int rh, int radius, Vec3 c, double a) {
    double hw = rw * 0.5, hh = rh * 0.5;
    double cx = x + hw, cy = y + hh;
    double r = std::min(double(radius), std::min(hw, hh));
    int x0 = std::max(0, x - 1), y0 = std::max(0, y - 1);
    int x1 = std::min(w, x + rw + 1), y1 = std::min(h, y + rh + 1);
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) {
            double d = sdRoundRect(xx + 0.5 - cx, yy + 0.5 - cy, hw, hh, r);
            double cov = clampd(0.5 - d, 0.0, 1.0);
            if (cov > 0) blend(xx, yy, c, cov * a);
        }
}

void Canvas::roundRectOutline(int x, int y, int rw, int rh, int radius, Vec3 c, double a) {
    double hw = rw * 0.5, hh = rh * 0.5;
    double cx = x + hw, cy = y + hh;
    double r = std::min(double(radius), std::min(hw, hh));
    int x0 = std::max(0, x - 1), y0 = std::max(0, y - 1);
    int x1 = std::min(w, x + rw + 1), y1 = std::min(h, y + rh + 1);
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) {
            double d = std::fabs(sdRoundRect(xx + 0.5 - cx, yy + 0.5 - cy, hw, hh, r));
            double cov = clampd(1.0 - d, 0.0, 1.0);
            if (cov > 0) blend(xx, yy, c, cov * a);
        }
}

void Canvas::line(double x0, double y0, double x1, double y1, Vec3 c, double a, double t) {
    double dx = x1 - x0, dy = y1 - y0;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-9) { circle(x0, y0, t * 0.5, c, a); return; }
    double half = std::max(0.5, t * 0.5);
    int bx0 = std::max(0, int(std::floor(std::min(x0, x1) - half - 1)));
    int bx1 = std::min(w - 1, int(std::ceil(std::max(x0, x1) + half + 1)));
    int by0 = std::max(0, int(std::floor(std::min(y0, y1) - half - 1)));
    int by1 = std::min(h - 1, int(std::ceil(std::max(y0, y1) + half + 1)));
    double ux = dx / len, uy = dy / len;

    for (int y = by0; y <= by1; y++)
        for (int x = bx0; x <= bx1; x++) {
            double rx = x + 0.5 - x0, ry = y + 0.5 - y0;
            double proj = clampd(rx * ux + ry * uy, 0.0, len);
            double px_ = rx - ux * proj, py_ = ry - uy * proj;
            double d = std::sqrt(px_ * px_ + py_ * py_);
            double cov = clampd(half + 0.5 - d, 0.0, 1.0);
            if (cov > 0) blend(x, y, c, cov * a);
        }
}

void Canvas::circle(double cx, double cy, double r, Vec3 c, double a) {
    int x0 = std::max(0, int(std::floor(cx - r - 1))), x1 = std::min(w - 1, int(std::ceil(cx + r + 1)));
    int y0 = std::max(0, int(std::floor(cy - r - 1))), y1 = std::min(h - 1, int(std::ceil(cy + r + 1)));
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
            double cov = clampd(r + 0.5 - std::sqrt(dx * dx + dy * dy), 0.0, 1.0);
            if (cov > 0) blend(x, y, c, cov * a);
        }
}

void Canvas::circleOutline(double cx, double cy, double r, Vec3 c, double a, double t) {
    double half = std::max(0.5, t * 0.5);
    int x0 = std::max(0, int(std::floor(cx - r - t - 1))), x1 = std::min(w - 1, int(std::ceil(cx + r + t + 1)));
    int y0 = std::max(0, int(std::floor(cy - r - t - 1))), y1 = std::min(h - 1, int(std::ceil(cy + r + t + 1)));
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
            double d = std::fabs(std::sqrt(dx * dx + dy * dy) - r);
            double cov = clampd(half + 0.5 - d, 0.0, 1.0);
            if (cov > 0) blend(x, y, c, cov * a);
        }
}

std::vector<uint8_t> Canvas::toRGB() const {
    std::vector<uint8_t> out(size_t(w) * h * 3);
    for (size_t i = 0; i < px.size(); i++) {
        out[i * 3 + 0] = uint8_t((px[i] >> 16) & 0xFF);
        out[i * 3 + 1] = uint8_t((px[i] >> 8) & 0xFF);
        out[i * 3 + 2] = uint8_t(px[i] & 0xFF);
    }
    return out;
}

// ------------------------------------------------------------------ text

namespace {

struct LoadedFace {
    const fontdata::FaceRec* rec = nullptr;
    std::vector<uint8_t> blob;
    std::unordered_map<int, const fontdata::GlyphRec*> map;
};

int base64Value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::vector<uint8_t> base64Decode(const char* s, int expectedBytes) {
    std::vector<uint8_t> out;
    out.reserve(size_t(expectedBytes));
    int acc = 0, bits = 0;
    for (const char* p = s; *p; p++) {
        int v = base64Value(*p);
        if (v < 0) continue;
        acc = (acc << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(uint8_t((acc >> bits) & 0xFF));
        }
    }
    return out;
}

LoadedFace& faceOf(Face f) {
    static LoadedFace faces[5];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < fontdata::kFaceCount && i < 5; i++) {
            faces[i].rec = &fontdata::kFaces[i];
            faces[i].blob = base64Decode(faces[i].rec->blobBase64, faces[i].rec->blobBytes);
            for (int g = 0; g < faces[i].rec->glyphCount; g++)
                faces[i].map[faces[i].rec->glyphs[g].cp] = &faces[i].rec->glyphs[g];
        }
        init = true;
    }
    return faces[int(f)];
}

// Minimal UTF-8 decoder: advances `i` and returns the codepoint.
int nextCodepoint(const std::string& s, size_t& i) {
    unsigned char c = uint8_t(s[i]);
    if (c < 0x80) { i += 1; return c; }
    if ((c >> 5) == 0x6 && i + 1 < s.size()) {
        int cp = ((c & 0x1F) << 6) | (uint8_t(s[i + 1]) & 0x3F);
        i += 2;
        return cp;
    }
    if ((c >> 4) == 0xE && i + 2 < s.size()) {
        int cp = ((c & 0x0F) << 12) | ((uint8_t(s[i + 1]) & 0x3F) << 6) | (uint8_t(s[i + 2]) & 0x3F);
        i += 3;
        return cp;
    }
    if ((c >> 3) == 0x1E && i + 3 < s.size()) { i += 4; return '?'; }
    i += 1;
    return '?';
}

}  // namespace

int textLineHeight(Face f) { return faceOf(f).rec->lineHeight; }
int textAscent(Face f) { return faceOf(f).rec->ascent; }

int textWidth(Face f, const std::string& s) {
    LoadedFace& lf = faceOf(f);
    double x = 0;
    for (size_t i = 0; i < s.size();) {
        int cp = nextCodepoint(s, i);
        auto it = lf.map.find(cp);
        if (it == lf.map.end()) it = lf.map.find('?');
        if (it != lf.map.end()) x += it->second->advance;
    }
    return int(x + 0.5);
}

void drawText(Canvas& cv, Face f, int x, int baselineY, const std::string& s, Vec3 c, double a) {
    LoadedFace& lf = faceOf(f);
    double pen = x;
    for (size_t i = 0; i < s.size();) {
        int cp = nextCodepoint(s, i);
        auto it = lf.map.find(cp);
        if (it == lf.map.end()) it = lf.map.find('?');
        if (it == lf.map.end()) continue;
        const fontdata::GlyphRec* g = it->second;

        int gx = int(pen + 0.5) + g->bearingX;
        int gy = baselineY + g->bearingY;
        for (int row = 0; row < g->h; row++) {
            int py = gy + row;
            if (py < 0 || py >= cv.h) continue;
            const uint8_t* src = lf.blob.data() + g->offset + size_t(row) * g->w;
            for (int col = 0; col < g->w; col++) {
                uint8_t cov = src[col];
                if (!cov) continue;
                cv.blend(gx + col, py, c, (cov / 255.0) * a);
            }
        }
        pen += g->advance;
    }
}

void drawTextTop(Canvas& cv, Face f, int x, int topY, const std::string& s, Vec3 c, double a) {
    drawText(cv, f, x, topY + textAscent(f), s, c, a);
}

}  // namespace sl
