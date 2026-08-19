// vecmath.h — small linear-algebra kit for the software renderer.
//
// Everything the pipeline needs and nothing else: 2D/3D/4D vectors, a
// column-vector 4x4 matrix (v' = M * v), and the standard camera/projection
// builders from the transformations lecture.
#pragma once

#include <cmath>
#include <algorithm>

namespace sl {

constexpr double PI = 3.14159265358979323846;

inline double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline double lerpd(double a, double b, double t) { return a + (b - a) * t; }
inline double radians(double deg) { return deg * PI / 180.0; }

// ---------------------------------------------------------------- Vec2

struct Vec2 {
    double x = 0, y = 0;
    Vec2() = default;
    Vec2(double x_, double y_) : x(x_), y(y_) {}
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, double s) { return {a.x * s, a.y * s}; }
inline double dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline double cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
inline double length(Vec2 a) { return std::sqrt(dot(a, a)); }

// ---------------------------------------------------------------- Vec3

struct Vec3 {
    double x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    explicit Vec3(double s) : x(s), y(s), z(s) {}
    double  operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
    double& operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
inline Vec3 operator*(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator*(double s, Vec3 a) { return a * s; }
inline Vec3 operator/(Vec3 a, double s) { return {a.x / s, a.y / s, a.z / s}; }
inline Vec3& operator+=(Vec3& a, Vec3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
inline Vec3& operator-=(Vec3& a, Vec3 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }
inline Vec3& operator*=(Vec3& a, double s) { a.x *= s; a.y *= s; a.z *= s; return a; }
inline Vec3& operator/=(Vec3& a, double s) { a.x /= s; a.y /= s; a.z /= s; return a; }

// Component-wise product — used for (light colour x material colour).
inline Vec3 mul(Vec3 a, Vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }

inline double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline double lengthSq(Vec3 a) { return dot(a, a); }
inline Vec3 normalize(Vec3 a) {
    double l = length(a);
    return l > 1e-15 ? a / l : Vec3(0, 0, 0);
}
inline Vec3 lerp(Vec3 a, Vec3 b, double t) { return a + (b - a) * t; }
inline Vec3 vmin(Vec3 a, Vec3 b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
inline Vec3 vmax(Vec3 a, Vec3 b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
inline Vec3 clamp01(Vec3 a) {
    return {clampd(a.x, 0, 1), clampd(a.y, 0, 1), clampd(a.z, 0, 1)};
}

// ---------------------------------------------------------------- Vec4

struct Vec4 {
    double x = 0, y = 0, z = 0, w = 0;
    Vec4() = default;
    Vec4(double x_, double y_, double z_, double w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vec4(Vec3 v, double w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
    Vec3 xyz() const { return {x, y, z}; }
};

inline Vec4 operator+(Vec4 a, Vec4 b) { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
inline Vec4 operator-(Vec4 a, Vec4 b) { return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }
inline Vec4 operator*(Vec4 a, double s) { return {a.x * s, a.y * s, a.z * s, a.w * s}; }
inline Vec4 lerp(Vec4 a, Vec4 b, double t) { return a + (b - a) * t; }

// ---------------------------------------------------------------- Mat4
//
// Row-major storage, column-vector convention: index m[row][col],
// transform with `m * v`, compose with `A * B` meaning "apply B, then A".

struct Mat4 {
    double m[4][4]{};

    static Mat4 identity() {
        Mat4 r;
        for (int i = 0; i < 4; i++) r.m[i][i] = 1.0;
        return r;
    }
    static Mat4 zero() { return Mat4{}; }

    static Mat4 translate(Vec3 t) {
        Mat4 r = identity();
        r.m[0][3] = t.x; r.m[1][3] = t.y; r.m[2][3] = t.z;
        return r;
    }
    static Mat4 scale(Vec3 s) {
        Mat4 r = identity();
        r.m[0][0] = s.x; r.m[1][1] = s.y; r.m[2][2] = s.z;
        return r;
    }
    static Mat4 rotateX(double a) {
        Mat4 r = identity();
        double c = std::cos(a), s = std::sin(a);
        r.m[1][1] = c; r.m[1][2] = -s; r.m[2][1] = s; r.m[2][2] = c;
        return r;
    }
    static Mat4 rotateY(double a) {
        Mat4 r = identity();
        double c = std::cos(a), s = std::sin(a);
        r.m[0][0] = c; r.m[0][2] = s; r.m[2][0] = -s; r.m[2][2] = c;
        return r;
    }
    static Mat4 rotateZ(double a) {
        Mat4 r = identity();
        double c = std::cos(a), s = std::sin(a);
        r.m[0][0] = c; r.m[0][1] = -s; r.m[1][0] = s; r.m[1][1] = c;
        return r;
    }

    // Right-handed look-at: builds the world -> camera (view) matrix.
    static Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
        Vec3 f = normalize(center - eye);   // forward
        Vec3 s = normalize(cross(f, up));   // right
        Vec3 u = cross(s, f);               // true up
        Mat4 r = identity();
        r.m[0][0] = s.x; r.m[0][1] = s.y; r.m[0][2] = s.z; r.m[0][3] = -dot(s, eye);
        r.m[1][0] = u.x; r.m[1][1] = u.y; r.m[1][2] = u.z; r.m[1][3] = -dot(u, eye);
        r.m[2][0] = -f.x; r.m[2][1] = -f.y; r.m[2][2] = -f.z; r.m[2][3] = dot(f, eye);
        return r;
    }

    // Perspective projection into clip space, mapping z in [-near,-far] -> w-scaled [-1,1].
    static Mat4 perspective(double fovYRadians, double aspect, double zNear, double zFar) {
        double t = 1.0 / std::tan(fovYRadians * 0.5);
        Mat4 r = zero();
        r.m[0][0] = t / aspect;
        r.m[1][1] = t;
        r.m[2][2] = (zFar + zNear) / (zNear - zFar);
        r.m[2][3] = (2.0 * zFar * zNear) / (zNear - zFar);
        r.m[3][2] = -1.0;
        return r;
    }

};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            double s = 0;
            for (int k = 0; k < 4; k++) s += a.m[i][k] * b.m[k][j];
            r.m[i][j] = s;
        }
    return r;
}

inline Vec4 operator*(const Mat4& a, const Vec4& v) {
    return {a.m[0][0] * v.x + a.m[0][1] * v.y + a.m[0][2] * v.z + a.m[0][3] * v.w,
            a.m[1][0] * v.x + a.m[1][1] * v.y + a.m[1][2] * v.z + a.m[1][3] * v.w,
            a.m[2][0] * v.x + a.m[2][1] * v.y + a.m[2][2] * v.z + a.m[2][3] * v.w,
            a.m[3][0] * v.x + a.m[3][1] * v.y + a.m[3][2] * v.z + a.m[3][3] * v.w};
}

}  // namespace sl
