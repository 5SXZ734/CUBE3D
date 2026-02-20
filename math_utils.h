// math_utils.h - Math types and utility functions
#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <cmath>
#include <algorithm>

// ==================== Vector Types ====================

struct Vec3 {
    float x, y, z;
};

struct Vec4 {
    float x, y, z, w;
};

// ==================== Matrix Type ====================

struct Mat4 {
    float m[16]; // column-major
};

// ==================== Vector Operations ====================

inline Vec3 v3_sub(Vec3 a, Vec3 b) { 
    return {a.x - b.x, a.y - b.y, a.z - b.z}; 
}

inline Vec3 v3_add(Vec3 a, Vec3 b) { 
    return {a.x + b.x, a.y + b.y, a.z + b.z}; 
}

inline Vec3 v3_scale(Vec3 v, float s) { 
    return {v.x * s, v.y * s, v.z * s}; 
}

inline float v3_dot(Vec3 a, Vec3 b) { 
    return a.x * b.x + a.y * b.y + a.z * b.z; 
}

inline Vec3 v3_cross(Vec3 a, Vec3 b) {
    return { 
        a.y * b.z - a.z * b.y, 
        a.z * b.x - a.x * b.z, 
        a.x * b.y - a.y * b.x 
    };
}

inline float v3_length(Vec3 v) {
    return std::sqrt(v3_dot(v, v));
}

inline Vec3 v3_norm(Vec3 v) {
    float len = std::sqrt((std::max)(1e-20f, v3_dot(v, v)));
    return { v.x / len, v.y / len, v.z / len };
}

// ==================== Matrix Operations ====================

inline Mat4 mat4_identity() {
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

inline Mat4 mat4_mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c)
    for (int row = 0; row < 4; ++row) {
        r.m[c * 4 + row] =
            a.m[0 * 4 + row] * b.m[c * 4 + 0] +
            a.m[1 * 4 + row] * b.m[c * 4 + 1] +
            a.m[2 * 4 + row] * b.m[c * 4 + 2] +
            a.m[3 * 4 + row] * b.m[c * 4 + 3];
    }
    return r;
}

inline Mat4 mat4_translate(float x, float y, float z) {
    Mat4 r = mat4_identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

inline Mat4 mat4_scale(float x, float y, float z) {
    Mat4 r = mat4_identity();
    r.m[0] = x;
    r.m[5] = y;
    r.m[10] = z;
    return r;
}

inline Mat4 mat4_rotate_x(float a) {
    Mat4 r = mat4_identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[5] = c;  r.m[9]  = -s;
    r.m[6] = s;  r.m[10] =  c;
    return r;
}

inline Mat4 mat4_rotate_y(float a) {
    Mat4 r = mat4_identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[0] =  c; r.m[8] = s;
    r.m[2] = -s; r.m[10] = c;
    return r;
}

inline Mat4 mat4_rotate_z(float a) {
    Mat4 r = mat4_identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[0] = c;  r.m[4] = -s;
    r.m[1] = s;  r.m[5] =  c;
    return r;
}

inline Mat4 mat4_lookAtRH(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = v3_norm(v3_sub(center, eye));
    Vec3 s = v3_norm(v3_cross(f, up));
    Vec3 u = v3_cross(s, f);

    Mat4 r = mat4_identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8]  = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9]  = u.z;
    r.m[2] =-f.x; r.m[6] =-f.y; r.m[10] =-f.z;

    r.m[12] = -v3_dot(s, eye);
    r.m[13] = -v3_dot(u, eye);
    r.m[14] =  v3_dot(f, eye);
    return r;
}

inline Mat4 mat4_perspectiveRH_NO(float fovyRadians, float aspect, float zNear, float zFar) {
    float f = 1.0f / std::tan(fovyRadians * 0.5f);

    Mat4 r{};
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (zFar + zNear) / (zNear - zFar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    return r;
}

#endif // MATH_UTILS_H
