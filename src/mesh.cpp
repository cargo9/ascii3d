#include "mesh.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <tuple>

namespace {

std::string lowerExt(const std::string& path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    std::string e = path.substr(dot + 1);
    for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return e;
}

std::string dirOf(const std::string& path) {
    auto slash = path.find_last_of('/');
    return slash == std::string::npos ? "" : path.substr(0, slash + 1);
}

// ---------- OBJ ----------

std::map<std::string, Vec3> loadMtl(const std::string& path) {
    std::map<std::string, Vec3> mats;
    std::ifstream f(path);
    std::string line, current;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "newmtl") {
            ss >> current;
            mats[current] = {0.85f, 0.85f, 0.85f};
        } else if (tag == "Kd" && !current.empty()) {
            Vec3 c;
            ss >> c.x >> c.y >> c.z;
            mats[current] = c;
        }
    }
    return mats;
}

bool loadObj(const std::string& path, Mesh& m, std::string& err) {
    std::ifstream f(path);
    if (!f) {
        err = "не удалось открыть файл";
        return false;
    }
    std::map<std::string, Vec3> mats;
    Vec3 color{0.85f, 0.85f, 0.85f};
    std::string line;
    std::vector<int> face;

    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "v") {
            Vec3 v;
            ss >> v.x >> v.y >> v.z;
            m.verts.push_back(v);
        } else if (tag == "f") {
            face.clear();
            std::string tok;
            while (ss >> tok) {
                // "7", "7/2", "7/2/5", "7//5"; отрицательные — с конца списка
                int idx = std::atoi(tok.c_str());
                if (idx < 0) idx = static_cast<int>(m.verts.size()) + idx;
                else idx -= 1;
                face.push_back(idx);
            }
            // многоугольник → веер треугольников
            for (size_t i = 1; i + 1 < face.size(); i++) {
                Triangle t{};
                t.v[0] = face[0];
                t.v[1] = face[i];
                t.v[2] = face[i + 1];
                t.color = color;
                m.tris.push_back(t);
            }
        } else if (tag == "mtllib") {
            std::string name;
            std::getline(ss >> std::ws, name);
            while (!name.empty() && (name.back() == '\r' || name.back() == ' ')) name.pop_back();
            auto more = loadMtl(dirOf(path) + name);
            mats.insert(more.begin(), more.end());
        } else if (tag == "usemtl") {
            std::string name;
            ss >> name;
            auto it = mats.find(name);
            color = it != mats.end() ? it->second : Vec3{0.85f, 0.85f, 0.85f};
        }
    }
    // выкинуть треугольники с битыми индексами
    int nv = static_cast<int>(m.verts.size());
    std::erase_if(m.tris, [nv](const Triangle& t) {
        for (int k = 0; k < 3; k++)
            if (t.v[k] < 0 || t.v[k] >= nv) return true;
        return false;
    });
    return true;
}

// ---------- STL ----------

struct VertexMerger {
    std::map<std::tuple<float, float, float>, int> index;
    Mesh& m;
    int add(Vec3 v) {
        // STL обычно Z-вверх (Blender), а у нас Y-вверх
        Vec3 p{v.x, v.z, -v.y};
        auto key = std::make_tuple(p.x, p.y, p.z);
        auto it = index.find(key);
        if (it != index.end()) return it->second;
        int i = static_cast<int>(m.verts.size());
        m.verts.push_back(p);
        index[key] = i;
        return i;
    }
};

bool loadStl(const std::string& path, Mesh& m, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        err = "не удалось открыть файл";
        return false;
    }
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    VertexMerger merger{{}, m};

    bool binary = false;
    if (data.size() >= 84) {
        uint32_t count;
        std::memcpy(&count, data.data() + 80, 4);
        binary = data.size() == 84 + static_cast<size_t>(count) * 50;
    }
    if (binary) {
        uint32_t count;
        std::memcpy(&count, data.data() + 80, 4);
        for (uint32_t i = 0; i < count; i++) {
            const char* p = data.data() + 84 + i * 50 + 12;  // пропустить нормаль
            Triangle t{};
            for (int k = 0; k < 3; k++) {
                float xyz[3];
                std::memcpy(xyz, p + k * 12, 12);
                t.v[k] = merger.add({xyz[0], xyz[1], xyz[2]});
            }
            m.tris.push_back(t);
        }
    } else {
        std::istringstream ss(data);
        std::string word;
        std::vector<int> corners;
        while (ss >> word) {
            if (word == "vertex") {
                Vec3 v;
                ss >> v.x >> v.y >> v.z;
                corners.push_back(merger.add(v));
                if (corners.size() == 3) {
                    Triangle t{};
                    t.v[0] = corners[0];
                    t.v[1] = corners[1];
                    t.v[2] = corners[2];
                    m.tris.push_back(t);
                    corners.clear();
                }
            } else if (word == "endloop") {
                corners.clear();
            }
        }
    }
    for (auto& t : m.tris) t.color = {0.85f, 0.85f, 0.85f};
    return true;
}

}  // namespace

bool loadMesh(const std::string& path, Mesh& out, std::string& err) {
    std::string ext = lowerExt(path);
    bool ok = false;
    if (ext == "obj") ok = loadObj(path, out, err);
    else if (ext == "stl") ok = loadStl(path, out, err);
    else err = "неизвестный формат 3D: ." + ext;
    if (ok && out.tris.empty()) {
        err = "в файле нет полигонов";
        ok = false;
    }
    return ok;
}

Mesh makeCube() {
    Mesh m;
    for (int i = 0; i < 8; i++)
        m.verts.push_back({(i & 1) ? 1.f : -1.f, (i & 2) ? 1.f : -1.f, (i & 4) ? 1.f : -1.f});
    // 6 граней по 2 треугольника, у каждой грани свой цвет
    const int faces[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1},
                             {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
    const Vec3 colors[6] = {{1.0f, 0.35f, 0.3f}, {0.3f, 0.8f, 0.4f}, {0.35f, 0.55f, 1.0f},
                            {1.0f, 0.85f, 0.3f}, {0.85f, 0.4f, 1.0f}, {0.3f, 0.9f, 0.95f}};
    for (int f = 0; f < 6; f++) {
        Triangle a{}, b{};
        a.v[0] = faces[f][0]; a.v[1] = faces[f][1]; a.v[2] = faces[f][2];
        b.v[0] = faces[f][0]; b.v[1] = faces[f][2]; b.v[2] = faces[f][3];
        a.color = b.color = colors[f];
        m.tris.push_back(a);
        m.tris.push_back(b);
    }
    return m;
}

Mesh makeTorus() {
    Mesh m;
    const int nu = 48, nv = 24;
    const float R = 1.0f, r = 0.45f;
    const float pi = 3.14159265f;
    for (int i = 0; i < nu; i++)
        for (int j = 0; j < nv; j++) {
            float u = 2 * pi * i / nu, v = 2 * pi * j / nv;
            m.verts.push_back({(R + r * std::cos(v)) * std::cos(u), r * std::sin(v),
                               (R + r * std::cos(v)) * std::sin(u)});
        }
    for (int i = 0; i < nu; i++)
        for (int j = 0; j < nv; j++) {
            int a = i * nv + j, b = ((i + 1) % nu) * nv + j;
            int c = ((i + 1) % nu) * nv + (j + 1) % nv, d = i * nv + (j + 1) % nv;
            // верх — глазурь, низ — тесто
            Vec3 col = std::sin(2 * pi * (j + 0.5f) / nv) > -0.1f ? Vec3{1.0f, 0.55f, 0.75f}
                                                                    : Vec3{0.85f, 0.6f, 0.35f};
            Triangle t1{}, t2{};
            t1.v[0] = a; t1.v[1] = d; t1.v[2] = c;
            t2.v[0] = a; t2.v[1] = c; t2.v[2] = b;
            t1.color = t2.color = col;
            m.tris.push_back(t1);
            m.tris.push_back(t2);
        }
    return m;
}

void prepareMesh(Mesh& m) {
    // центр по габаритам и масштаб в шар радиуса 1
    Vec3 lo = m.verts[0], hi = m.verts[0];
    for (auto& v : m.verts) {
        lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
        hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
    }
    Vec3 c = (lo + hi) * 0.5f;
    float rad = 0;
    for (auto& v : m.verts) rad = std::max(rad, length(v - c));
    if (rad < 1e-9f) rad = 1;
    for (auto& v : m.verts) v = (v - c) * (1.0f / rad);

    // Нормали: усредняем соседние грани, но только если угол между ними < 40°.
    // Так шар выглядит гладким, а у куба остаются острые рёбра.
    std::vector<Vec3> faceN(m.tris.size());
    std::vector<std::vector<int>> adj(m.verts.size());
    for (size_t i = 0; i < m.tris.size(); i++) {
        auto& t = m.tris[i];
        faceN[i] = normalize(cross(m.verts[t.v[1]] - m.verts[t.v[0]], m.verts[t.v[2]] - m.verts[t.v[0]]));
        for (int k = 0; k < 3; k++) adj[t.v[k]].push_back(static_cast<int>(i));
    }
    const float cosLimit = std::cos(40.0f * 3.14159265f / 180.0f);
    for (size_t i = 0; i < m.tris.size(); i++) {
        auto& t = m.tris[i];
        for (int k = 0; k < 3; k++) {
            Vec3 sum{};
            for (int j : adj[t.v[k]])
                if (dot(faceN[i], faceN[j]) >= cosLimit) sum += faceN[j];
            Vec3 n = normalize(sum);
            t.n[k] = length(n) > 0 ? n : faceN[i];
        }
    }
}
