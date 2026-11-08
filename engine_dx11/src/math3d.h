#pragma once
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#ifdef _WIN32
#ifndef _TARGET_PC_WIN
#define _TARGET_PC_WIN 1
#endif
#else
#ifndef _TARGET_PC_LINUX
#define _TARGET_PC_LINUX 1
#endif
#endif
#ifndef _TARGET_64BIT
#define _TARGET_64BIT 1
#endif
#ifndef _TARGET_SIMD_SSE
#define _TARGET_SIMD_SSE 4
#endif

#include <supp/dag_define_KRNLIMP.h>
#include <math/dag_Point3.h>
#include <math/dag_Point4.h>
#include <math/dag_TMatrix.h>

#ifdef PI
#undef PI
#endif

#include <cmath>
#include <algorithm>

namespace sf {

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 6.28318530717958647692f;

inline float deg2rad(float d) { return d * (PI / 180.0f); }
inline float rad2deg(float r) { return r * (180.0f / PI); }

inline float wrapAngle(float a) {
    while (a > PI) a -= TWO_PI;
    while (a < -PI) a += TWO_PI;
    return a;
}

inline float moveAngleToward(float curr, float target, float maxStep) {
    float diff = wrapAngle(target - curr);
    if (std::fabs(diff) <= maxStep) return target;
    return wrapAngle(curr + (diff > 0.0f ? maxStep : -maxStep));
}

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float lerpf(float a, float b, float t) {
    return a + (b - a) * clampf(t, 0.0f, 1.0f);
}

struct Vec2 {
    float x = 0.0f, y = 0.0f;
    Vec2() = default;
    Vec2(float _x, float _y) : x(_x), y(_y) {}
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vec3() = default;
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    Vec3(const Point3& p) : x(p.x), y(p.y), z(p.z) {}
    Point3 toDagor() const { return Point3(x, y, z); }

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { float inv = 1.0f / s; return {x * inv, y * inv, z * inv}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }

    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }
    Vec3 normalized() const {
        float l = length();
        return l > 1e-6f ? (*this / l) : Vec3(0.0f, 1.0f, 0.0f);
    }
    static float dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static Vec3 cross(const Vec3& a, const Vec3& b) {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }
    static Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
        return a + (b - a) * clampf(t, 0.0f, 1.0f);
    }
};

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    Vec4() = default;
    Vec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
};

// Row-major 4x4 matrix (HLSL row_major float4x4 compatible)
struct Mat4 {
    float m[4][4] = {};

    static Mat4 identity() {
        Mat4 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
        return r;
    }

    static Mat4 translation(float tx, float ty, float tz) {
        Mat4 r = identity();
        r.m[3][0] = tx; r.m[3][1] = ty; r.m[3][2] = tz;
        return r;
    }

    static Mat4 scale(float sx, float sy, float sz) {
        Mat4 r = identity();
        r.m[0][0] = sx; r.m[1][1] = sy; r.m[2][2] = sz;
        return r;
    }
    static Mat4 scaling(float sx, float sy, float sz) { return scale(sx, sy, sz); }

    static Mat4 rotX(float a) {
        Mat4 r = identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[1][1] = c;  r.m[1][2] = s;
        r.m[2][1] = -s; r.m[2][2] = c;
        return r;
    }
    static Mat4 rotationX(float a) { return rotX(a); }

    static Mat4 rotY(float a) {
        Mat4 r = identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[0][0] = c;  r.m[0][2] = -s;
        r.m[2][0] = s;  r.m[2][2] = c;
        return r;
    }
    static Mat4 rotationY(float a) { return rotY(a); }

    static Mat4 rotZ(float a) {
        Mat4 r = identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[0][0] = c;  r.m[0][1] = s;
        r.m[1][0] = -s; r.m[1][1] = c;
        return r;
    }
    static Mat4 rotationZ(float a) { return rotZ(a); }

    Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                r.m[i][j] = m[i][0] * b.m[0][j] +
                            m[i][1] * b.m[1][j] +
                            m[i][2] * b.m[2][j] +
                            m[i][3] * b.m[3][j];
            }
        }
        return r;
    }

    Vec3 transformPoint(const Vec3& p) const {
        return {
            p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + m[3][0],
            p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + m[3][1],
            p.x * m[0][2] + p.y * m[1][2] + p.z * m[2][2] + m[3][2]
        };
    }

    Vec3 transformNormal(const Vec3& n) const {
        return Vec3(
            n.x * m[0][0] + n.y * m[1][0] + n.z * m[2][0],
            n.x * m[0][1] + n.y * m[1][1] + n.z * m[2][1],
            n.x * m[0][2] + n.y * m[1][2] + n.z * m[2][2]
        ).normalized();
    }
    Vec3 transformDir(const Vec3& n) const { return transformNormal(n); }

    Vec4 mulVec4(const Vec4& v) const {
        return {
            v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + v.w * m[3][0],
            v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + v.w * m[3][1],
            v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + v.w * m[3][2],
            v.x * m[0][3] + v.y * m[1][3] + v.z * m[2][3] + v.w * m[3][3]
        };
    }

    static Mat4 lookAtLH(const Vec3& eye, const Vec3& at, const Vec3& up) {
        Vec3 zaxis = (at - eye).normalized();
        Vec3 xaxis = Vec3::cross(up, zaxis).normalized();
        Vec3 yaxis = Vec3::cross(zaxis, xaxis);

        Mat4 r = identity();
        r.m[0][0] = xaxis.x; r.m[0][1] = yaxis.x; r.m[0][2] = zaxis.x;
        r.m[1][0] = xaxis.y; r.m[1][1] = yaxis.y; r.m[1][2] = zaxis.y;
        r.m[2][0] = xaxis.z; r.m[2][1] = yaxis.z; r.m[2][2] = zaxis.z;
        r.m[3][0] = -Vec3::dot(xaxis, eye);
        r.m[3][1] = -Vec3::dot(yaxis, eye);
        r.m[3][2] = -Vec3::dot(zaxis, eye);
        return r;
    }

    static Mat4 perspectiveFovLH(float fovY, float aspect, float zn, float zf) {
        float yScale = 1.0f / std::tan(fovY * 0.5f);
        float xScale = yScale / aspect;
        Mat4 r;
        r.m[0][0] = xScale;
        r.m[1][1] = yScale;
        r.m[2][2] = zf / (zf - zn);
        r.m[2][3] = 1.0f;
        r.m[3][2] = -zn * zf / (zf - zn);
        return r;
    }

    static Mat4 orthoLH(float w, float h, float zn, float zf) {
        Mat4 r = identity();
        r.m[0][0] = 2.0f / w;
        r.m[1][1] = 2.0f / h;
        r.m[2][2] = 1.0f / (zf - zn);
        r.m[3][2] = -zn / (zf - zn);
        return r;
    }
};

inline bool projectWorldToScreen(const Vec3& world, const Mat4& viewProj, float screenW, float screenH, float& outX, float& outY) {
    Vec4 clip = viewProj.mulVec4(Vec4(world.x, world.y, world.z, 1.0f));
    if (clip.w <= 0.05f) return false;
    float ndcX = clip.x / clip.w;
    float ndcY = clip.y / clip.w;
    float ndcZ = clip.z / clip.w;
    if (ndcZ < 0.0f || ndcZ > 1.0f) return false;
    outX = (ndcX * 0.5f + 0.5f) * screenW;
    outY = (-ndcY * 0.5f + 0.5f) * screenH;
    return true;
}

inline bool projectWorldToScreen(const Vec3& world, const Mat4& viewProj, float screenW, float screenH, Vec2& outPt) {
    return projectWorldToScreen(world, viewProj, screenW, screenH, outPt.x, outPt.y);
}

} // namespace sf
