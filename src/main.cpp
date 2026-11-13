// ascii3d — картинки и 3D-модели в терминале
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "image.h"
#include "mesh.h"
#include "term.h"
#include "viewer3d.h"

namespace {

void usage() {
    std::cout <<
        "Использование: ab [опции] <файл>\n"
        "\n"
        "  Картинки: jpg png bmp gif tga psd hdr svg (+ webp/heic/avif через ImageMagick)\n"
        "  3D:       obj stl  (Blender: File → Export → Wavefront .obj или .stl)\n"
        "  Без файла — крутится пончик. Демо: --donut, --cube\n"
        "\n"
        "Опции:\n"
        "  -w N        ширина в символах\n"
        "  -H N        высота в строках (только 3D с --once)\n"
        "  -b          картинка полублоками ▀ (чётче, но не ASCII)\n"
        "  --mono      без цвета\n"
        "  --invert    для светлого фона терминала\n"
        "  --white     прозрачные места картинки — белые\n"
        "  -p          картинку просто напечатать (остаётся в истории терминала)\n"
        "  --once      3D: один кадр и выход (удобно для скриншота)\n"
        "  -h          эта справка\n"
        "\n"
        "Клавиши в 3D: ←↑↓→/WASD вращать, колесо или +/- зум, пробел пауза, c цвет, r сброс, q выход\n"
        "Картинка:     Ctrl+колесо или +/- размер символов, b блоки, c цвет, i инверсия, q выход\n"
        "Ctrl+колесо меняет шрифт Alacritty (после выхода шрифт возвращается)\n";
}

std::string lowerExt(const std::string& path) {
    std::string e = std::filesystem::path(path).extension().string();
    if (!e.empty()) e.erase(0, 1);
    for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return e;
}

bool readInt(int argc, char** argv, int& i, int& out) {
    if (i + 1 >= argc) return false;
    out = std::atoi(argv[++i]);
    return out > 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string file;
    ImageOptions io;
    ViewOptions vo;
    std::string demo = "donut";
    bool printOnly = false;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { usage(); return 0; }
        else if (a == "-w") { if (!readInt(argc, argv, i, io.width)) { std::cerr << "-w: нужно число\n"; return 2; } vo.width = io.width; }
        else if (a == "-H") { if (!readInt(argc, argv, i, vo.height)) { std::cerr << "-H: нужно число\n"; return 2; } }
        else if (a == "-b" || a == "--blocks") io.blocks = true;
        else if (a == "--mono") io.color = vo.color = false;
        else if (a == "--invert") io.invert = true;
        else if (a == "--white") io.whiteBg = true;
        else if (a == "--once") vo.once = true;
        else if (a == "-p" || a == "--print") printOnly = true;
        else if (a == "--cube") demo = "cube";
        else if (a == "--donut") demo = "donut";
        else if (a.size() > 1 && a[0] == '-') { std::cerr << "Неизвестная опция: " << a << "  (ab -h — справка)\n"; return 2; }
        else file = a;
    }

    // Без файла — демо-модель
    if (file.empty()) {
        Mesh m = demo == "cube" ? makeCube() : makeTorus();
        prepareMesh(m);
        return runViewer(m, vo, demo);
    }

    if (!std::filesystem::exists(file)) {
        std::cerr << "Файл не найден: " << file << "\n";
        return 1;
    }
    std::string name = std::filesystem::path(file).filename().string();
    std::string ext = lowerExt(file);
    std::string err;

    if (ext == "obj" || ext == "stl") {
        Mesh m;
        if (!loadMesh(file, m, err)) {
            std::cerr << name << ": " << err << "\n";
            return 1;
        }
        prepareMesh(m);
        return runViewer(m, vo, name);
    }

    Image img;
    if (!loadImage(file, img, err)) {
        std::cerr << name << ": " << err << "\n";
        if (ext == "blend") std::cerr << "Это файл Blender — экспортируй модель: File → Export → Wavefront (.obj)\n";
        else if (ext == "fbx" || ext == "glb" || ext == "gltf" || ext == "3ds" || ext == "dae")
            std::cerr << "Этот 3D-формат не поддерживается — экспортируй в .obj или .stl\n";
        return 1;
    }
    if (term::isTty() && !printOnly) return runImageViewer(img, io, name);

    term::Size ts = term::isTty() ? term::size() : term::Size{100, 0};
    std::cout << renderImage(img, io, ts.cols, term::isTty() ? ts.rows - 1 : 0);
    return 0;
}
