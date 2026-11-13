#pragma once
#include <string>
#include <vector>

struct Image {
    int w = 0, h = 0;
    std::vector<unsigned char> rgba;  // w*h*4 байт
};

struct ImageOptions {
    int width = 0;          // ширина в символах; 0 = по терминалу
    bool color = true;      // truecolor
    bool blocks = false;    // полублоки ▀ вместо ASCII (в 2 раза чётче)
    bool invert = false;    // для светлого фона терминала
    bool whiteBg = false;   // прозрачность заливать белым, а не чёрным
};

// jpg/png/bmp/gif/tga/psd/hdr/pnm (stb_image), svg (nanosvg),
// остальное (webp, heic, avif…) — через ImageMagick, если установлен.
bool loadImage(const std::string& path, Image& out, std::string& err);

// maxRows = 0 — без ограничения по высоте. В outCols/outRows (если не null) —
// сколько клеток реально заняла картинка.
std::string renderImage(const Image& img, const ImageOptions& opt, int maxCols, int maxRows,
                        int* outCols = nullptr, int* outRows = nullptr);

// Полноэкранный просмотр: подгоняется под окно, перерисовывается при смене размера/шрифта
int runImageViewer(const Image& img, ImageOptions opt, const std::string& title);
