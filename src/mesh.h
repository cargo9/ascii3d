#pragma once
#include <string>
#include <vector>

#include "vec.h"

struct Triangle {
    int v[3];          // индексы вершин
    Vec3 n[3];         // нормали в углах (для гладкого освещения)
    Vec3 color{0.85f, 0.85f, 0.85f};
};

struct Mesh {
    std::vector<Vec3> verts;
    std::vector<Triangle> tris;
};

// .obj (с цветами из .mtl) и .stl (бинарный и текстовый)
bool loadMesh(const std::string& path, Mesh& out, std::string& err);

Mesh makeCube();
Mesh makeTorus();

// Сдвинуть в центр, вписать в шар радиуса 1, посчитать нормали
void prepareMesh(Mesh& m);
