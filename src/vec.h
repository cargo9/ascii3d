#pragma once
#include <cmath>

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator*(float s, Vec3 a) { return a * s; }
inline Vec3& operator+=(Vec3& a, Vec3 b) { a = a + b; return a; }

inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(Vec3 a) {
    float l = length(a);
    return l > 1e-12f ? a * (1.0f / l) : Vec3{0, 0, 0};
}

// Матрица 3×3 для поворотов
struct Mat3 {
    float m[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

    static Mat3 rotX(float a) {
        float c = std::cos(a), s = std::sin(a);
        Mat3 r;
        r.m[1][1] = c; r.m[1][2] = -s;
        r.m[2][1] = s; r.m[2][2] = c;
        return r;
    }
    static Mat3 rotY(float a) {
        float c = std::cos(a), s = std::sin(a);
        Mat3 r;
        r.m[0][0] = c;  r.m[0][2] = s;
        r.m[2][0] = -s; r.m[2][2] = c;
        return r;
    }
};

inline Vec3 operator*(const Mat3& M, Vec3 v) {
    return {M.m[0][0] * v.x + M.m[0][1] * v.y + M.m[0][2] * v.z,
            M.m[1][0] * v.x + M.m[1][1] * v.y + M.m[1][2] * v.z,
            M.m[2][0] * v.x + M.m[2][1] * v.y + M.m[2][2] * v.z};
}

// После тысяч умножений матрица «плывёт» — выравниваем строки обратно
inline Mat3 orthonormalize(const Mat3& M) {
    Vec3 r0{M.m[0][0], M.m[0][1], M.m[0][2]};
    Vec3 r1{M.m[1][0], M.m[1][1], M.m[1][2]};
    r0 = normalize(r0);
    r1 = normalize(r1 - r0 * dot(r0, r1));
    Vec3 r2 = cross(r0, r1);
    Mat3 r;
    const Vec3 rows[3] = {r0, r1, r2};
    for (int i = 0; i < 3; i++) {
        r.m[i][0] = rows[i].x;
        r.m[i][1] = rows[i].y;
        r.m[i][2] = rows[i].z;
    }
    return r;
}

inline Mat3 operator*(const Mat3& A, const Mat3& B) {
    Mat3 r;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            r.m[i][j] = 0;
            for (int k = 0; k < 3; k++) r.m[i][j] += A.m[i][k] * B.m[k][j];
        }
    return r;
}
