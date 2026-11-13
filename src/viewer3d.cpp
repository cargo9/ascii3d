#include "viewer3d.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

#include "fontzoom.h"
#include "term.h"

namespace {

// Поворот модели храним одной матрицей и докручиваем её маленькими поворотами
// относительно ЭКРАНА: ←→ всегда крутит вокруг вертикали экрана, ↑↓ — вокруг горизонтали.
struct Camera {
    Mat3 orient = Mat3::rotX(-0.45f) * Mat3::rotY(0.6f);  // слегка сверху и сбоку
    float zoom = 1.0f;
    bool spin = true;

    void rotate(float aroundVertical, float aroundHorizontal) {
        orient = orthonormalize(Mat3::rotX(aroundHorizontal) * Mat3::rotY(aroundVertical) * orient);
    }
};

struct ScreenPt {
    float x, y, invz;
};

const std::string kRamp = ".,-~:;=!*#$@";

// Нарисовать модель в W×H клеток и вернуть готовый текст с ANSI-цветами
std::string renderFrame(const Mesh& mesh, const Camera& cam, int W, int H, bool color) {
    const float dist = 3.0f;               // камера в (0,0,0), смотрит вдоль +z
    const float f = 2.6f * cam.zoom;       // фокус (≈ угол обзора 42°)
    const float scale = std::min(W, 2 * H) * 0.5f;  // клетка в 2 раза выше, чем шире
    const float cx = W * 0.5f, cy = H * 0.5f;
    const Vec3 light = normalize({-0.5f, 0.7f, -0.6f});
    const Vec3 halfway = normalize(light + Vec3{0, 0, -1});

    const Mat3& R = cam.orient;

    std::vector<Vec3> P(mesh.verts.size());
    std::vector<ScreenPt> S(mesh.verts.size());
    for (size_t i = 0; i < mesh.verts.size(); i++) {
        Vec3 p = R * mesh.verts[i];
        p.z += dist;
        P[i] = p;
        float iz = 1.0f / std::max(p.z, 1e-4f);
        S[i] = {cx + p.x * f * iz * scale, cy - p.y * f * iz * scale * 0.5f, iz};
    }

    std::vector<char> chars(static_cast<size_t>(W) * H, ' ');
    std::vector<Vec3> cols(static_cast<size_t>(W) * H);
    std::vector<float> zbuf(static_cast<size_t>(W) * H, 0.0f);  // хранит 1/z: больше = ближе

    auto edge = [](ScreenPt a, ScreenPt b, float px, float py) {
        return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
    };

    for (const Triangle& t : mesh.tris) {
        const Vec3 &p0 = P[t.v[0]], &p1 = P[t.v[1]], &p2 = P[t.v[2]];
        if (p0.z < 0.1f || p1.z < 0.1f || p2.z < 0.1f) continue;  // за камерой
        ScreenPt s0 = S[t.v[0]], s1 = S[t.v[1]], s2 = S[t.v[2]];
        float area = edge(s0, s1, s2.x, s2.y);
        if (std::fabs(area) < 1e-9f) continue;

        int minX = std::max(0, static_cast<int>(std::floor(std::min({s0.x, s1.x, s2.x}))));
        int maxX = std::min(W - 1, static_cast<int>(std::ceil(std::max({s0.x, s1.x, s2.x}))));
        int minY = std::max(0, static_cast<int>(std::floor(std::min({s0.y, s1.y, s2.y}))));
        int maxY = std::min(H - 1, static_cast<int>(std::ceil(std::max({s0.y, s1.y, s2.y}))));
        if (minX > maxX || minY > maxY) continue;

        // если грань смотрит от камеры — освещаем её обратную сторону
        Vec3 fn = cross(p1 - p0, p2 - p0);
        float side = dot(fn, p0) > 0 ? -1.0f : 1.0f;
        Vec3 n0 = R * t.n[0] * side, n1 = R * t.n[1] * side, n2 = R * t.n[2] * side;

        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                float px = x + 0.5f, py = y + 0.5f;
                float w0 = edge(s1, s2, px, py) / area;
                float w1 = edge(s2, s0, px, py) / area;
                float w2 = 1.0f - w0 - w1;
                if (w0 < -1e-5f || w1 < -1e-5f || w2 < -1e-5f) continue;

                float iz = w0 * s0.invz + w1 * s1.invz + w2 * s2.invz;
                size_t idx = static_cast<size_t>(y) * W + x;
                if (iz <= zbuf[idx]) continue;  // z-буфер: тут уже есть что-то ближе
                zbuf[idx] = iz;

                Vec3 n = normalize(n0 * w0 + n1 * w1 + n2 * w2);
                float diff = std::max(0.0f, dot(n, light));
                float spec = std::pow(std::max(0.0f, dot(n, halfway)), 24.0f) * 0.5f;
                float lum = std::clamp(0.06f + 0.8f * diff + spec, 0.0f, 1.0f);

                chars[idx] = kRamp[static_cast<size_t>(lum * (kRamp.size() - 1) + 0.5f)];
                cols[idx] = t.color * (0.18f + 0.82f * diff) + Vec3{1, 1, 1} * spec;
            }
        }
    }

    std::string out;
    out.reserve(static_cast<size_t>(W) * H * (color ? 22 : 1) + H * 8);
    for (int y = 0; y < H; y++) {
        int lr = -1, lg = -1, lb = -1;
        for (int x = 0; x < W; x++) {
            size_t idx = static_cast<size_t>(y) * W + x;
            char c = chars[idx];
            if (color && c != ' ') {
                int r = std::clamp(static_cast<int>(cols[idx].x * 255), 0, 255);
                int g = std::clamp(static_cast<int>(cols[idx].y * 255), 0, 255);
                int b = std::clamp(static_cast<int>(cols[idx].z * 255), 0, 255);
                if (r != lr || g != lg || b != lb) {
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "\033[38;2;%d;%d;%dm", r, g, b);
                    out += buf;
                    lr = r; lg = g; lb = b;
                }
            }
            out += c;
        }
        if (color) out += "\033[0m";
        if (y + 1 < H) out += '\n';
    }
    return out;
}

}  // namespace

int runViewer(const Mesh& mesh, const ViewOptions& opt, const std::string& title) {
    Camera cam;

    // Один кадр — для pipe/файла или с флагом --once
    if (opt.once || !term::isTty()) {
        term::Size ts = term::isTty() ? term::size() : term::Size{80, 0};
        int W = opt.width > 0 ? opt.width : ts.cols;
        int H = opt.height > 0 ? opt.height : (ts.rows > 1 ? ts.rows - 1 : W * 2 / 5);
        term::write(renderFrame(mesh, cam, W, H, opt.color) + "\n");
        return 0;
    }

    bool color = opt.color;
    FontZoom font;
    term::enterInteractive();
    term::Size last{0, 0};
    auto prev = std::chrono::steady_clock::now();
    const auto frameTime = std::chrono::milliseconds(33);  // ~30 FPS

    char info[320];
    std::snprintf(info, sizeof(info),
                  " %s · %zu треуг. · ←↑↓→/wasd вращать · колесо/+- зум%s · пробел пауза · c цвет · r сброс · q выход",
                  title.c_str(), mesh.tris.size(), font.available() ? " · Ctrl+колесо размер символов" : "");

    bool running = true;
    while (running && !term::quitRequested()) {
        auto now = std::chrono::steady_clock::now();
        float dt = std::min(0.1f, std::chrono::duration<float>(now - prev).count());
        prev = now;

        float turnV = 0, turnH = 0;
        int fontSteps = 0;
        for (auto k : term::readKeys()) {
            using term::Key;
            char ch = static_cast<char>(std::tolower(static_cast<unsigned char>(k.ch)));
            if (k.key == Key::Escape || (k.key == Key::Char && ch == 'q')) running = false;
            else if (k.key == Key::Left || ch == 'a') turnV -= 0.2f;
            else if (k.key == Key::Right || ch == 'd') turnV += 0.2f;
            else if (k.key == Key::Up || ch == 'w') turnH -= 0.2f;
            else if (k.key == Key::Down || ch == 's') turnH += 0.2f;
            else if (k.key == Key::WheelUp) { if (k.ctrl) fontSteps++; else cam.zoom *= 1.1f; }
            else if (k.key == Key::WheelDown) { if (k.ctrl) fontSteps--; else cam.zoom /= 1.1f; }
            else if (ch == '+' || ch == '=') cam.zoom *= 1.12f;
            else if (ch == '-' || ch == '_') cam.zoom /= 1.12f;
            else if (ch == ' ') cam.spin = !cam.spin;
            else if (ch == 'c') color = !color;
            else if (ch == 'r') cam = Camera{};
        }
        cam.zoom = std::clamp(cam.zoom, 0.2f, 8.0f);
        // Ctrl+колесо как в браузере: вверх — символы крупнее, вниз — мельче (больше деталей)
        font.change(fontSteps);
        if (cam.spin) {  // кувыркаемся сразу по двум осям — так видно вращение даже у шара
            turnV += 0.9f * dt;
            turnH += 0.4f * dt;
        }
        if (turnV != 0 || turnH != 0) cam.rotate(turnV, turnH);

        term::Size ts = term::size();
        std::string frame;
        if (ts.cols != last.cols || ts.rows != last.rows) {
            frame += "\033[2J";  // окно поменяло размер — стереть старое
            last = ts;
        }
        int H = std::max(1, ts.rows - 1);  // последняя строка — подсказка
        frame += "\033[H";
        frame += renderFrame(mesh, cam, ts.cols, H, color);
        frame += "\n";
        frame += term::statusLine(info, ts.cols);
        term::write(frame);

        std::this_thread::sleep_until(now + frameTime);
    }
    font.restore();
    term::leaveInteractive();
    return 0;
}
