#include "image.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "nanosvg.h"
#include "nanosvgrast.h"
#include "stb_image.h"

namespace {

std::string lowerExt(const std::string& path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    std::string e = path.substr(dot + 1);
    for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return e;
}

bool loadSvg(const std::string& path, Image& out, std::string& err) {
    NSVGimage* svg = nsvgParseFromFile(path.c_str(), "px", 96.0f);
    if (!svg || svg->width <= 0 || svg->height <= 0) {
        if (svg) nsvgDelete(svg);
        err = "не удалось разобрать SVG";
        return false;
    }
    float scale = 1024.0f / std::max(svg->width, svg->height);
    out.w = std::max(1, static_cast<int>(std::ceil(svg->width * scale)));
    out.h = std::max(1, static_cast<int>(std::ceil(svg->height * scale)));
    out.rgba.assign(static_cast<size_t>(out.w) * out.h * 4, 0);
    NSVGrasterizer* rast = nsvgCreateRasterizer();
    nsvgRasterize(rast, svg, 0, 0, scale, out.rgba.data(), out.w, out.h, out.w * 4);
    nsvgDeleteRasterizer(rast);
    nsvgDelete(svg);
    return true;
}

bool fromMemory(const std::vector<unsigned char>& bytes, Image& out) {
    int w, h, n;
    unsigned char* data = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &n, 4);
    if (!data) return false;
    out.w = w;
    out.h = h;
    out.rgba.assign(data, data + static_cast<size_t>(w) * h * 4);
    stbi_image_free(data);
    return true;
}

// Запасной путь: попросить ImageMagick перегнать файл в PNG
bool loadViaMagick(const std::string& path, Image& out) {
    std::string quoted = "'";
    for (char c : path) {
        if (c == '\'') quoted += "'\\''";
        else quoted += c;
    }
    quoted += "'";
    std::string cmd = "magick " + quoted + "[0] png:- 2>/dev/null";
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return false;
    std::vector<unsigned char> bytes;
    unsigned char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), p)) > 0) bytes.insert(bytes.end(), buf, buf + n);
    pclose(p);
    return !bytes.empty() && fromMemory(bytes, out);
}

struct RGB {
    float r, g, b;
};

// Средний цвет прямоугольника пикселей [x0,x1)×[y0,y1), прозрачность смешана с фоном
RGB average(const Image& img, double x0, double y0, double x1, double y1, float bg) {
    int ix0 = std::clamp(static_cast<int>(std::floor(x0)), 0, img.w - 1);
    int iy0 = std::clamp(static_cast<int>(std::floor(y0)), 0, img.h - 1);
    int ix1 = std::clamp(static_cast<int>(std::ceil(x1)), ix0 + 1, img.w);
    int iy1 = std::clamp(static_cast<int>(std::ceil(y1)), iy0 + 1, img.h);
    double r = 0, g = 0, b = 0;
    int count = 0;
    for (int y = iy0; y < iy1; y++) {
        const unsigned char* p = &img.rgba[(static_cast<size_t>(y) * img.w + ix0) * 4];
        for (int x = ix0; x < ix1; x++, p += 4) {
            double a = p[3] / 255.0;
            r += p[0] * a + bg * (1 - a);
            g += p[1] * a + bg * (1 - a);
            b += p[2] * a + bg * (1 - a);
            count++;
        }
    }
    return {static_cast<float>(r / count), static_cast<float>(g / count), static_cast<float>(b / count)};
}

void appendColor(std::string& s, bool background, RGB c) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "\033[%d;2;%d;%d;%dm", background ? 48 : 38,
                  static_cast<int>(c.r), static_cast<int>(c.g), static_cast<int>(c.b));
    s += buf;
}

}  // namespace

bool loadImage(const std::string& path, Image& out, std::string& err) {
    if (lowerExt(path) == "svg") return loadSvg(path, out, err);

    int w, h, n;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (data) {
        out.w = w;
        out.h = h;
        out.rgba.assign(data, data + static_cast<size_t>(w) * h * 4);
        stbi_image_free(data);
        return true;
    }
    std::string stbErr = stbi_failure_reason() ? stbi_failure_reason() : "?";
    if (loadViaMagick(path, out)) return true;
    err = "не удалось открыть картинку (" + stbErr + ")";
    return false;
}

std::string renderImage(const Image& img, const ImageOptions& opt, int maxCols, int maxRows,
                        int* outCols, int* outRows) {
    // Клетка терминала примерно в 2 раза выше, чем шире
    int cols = opt.width > 0 ? opt.width : maxCols;
    int rows = std::max(1, static_cast<int>(std::lround(static_cast<double>(img.h) * cols / img.w / 2.0)));
    if (opt.width <= 0 && maxRows > 0 && rows > maxRows) {
        rows = maxRows;
        cols = std::max(1, static_cast<int>(std::lround(static_cast<double>(img.w) * rows * 2.0 / img.h)));
    }
    if (outCols) *outCols = cols;
    if (outRows) *outRows = rows;
    double cw = static_cast<double>(img.w) / cols;
    double ch = static_cast<double>(img.h) / rows;
    float bg = opt.whiteBg ? 255.0f : 0.0f;

    static const std::string ramp = " .,-~:;=!*#$@";
    std::string out;
    out.reserve(static_cast<size_t>(cols) * rows * (opt.color ? 20 : 1));

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            double x0 = x * cw, x1 = (x + 1) * cw;
            double y0 = y * ch, y1 = (y + 1) * ch;
            if (opt.blocks) {
                // ▀: верхняя половина клетки — цвет текста, нижняя — цвет фона
                double ym = (y0 + y1) / 2;
                RGB top = average(img, x0, y0, x1, ym, bg);
                RGB bot = average(img, x0, ym, x1, y1, bg);
                appendColor(out, false, top);
                appendColor(out, true, bot);
                out += "▀";
            } else {
                RGB c = average(img, x0, y0, x1, y1, bg);
                float lum = (0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b) / 255.0f;
                if (opt.invert) lum = 1.0f - lum;
                int idx = std::clamp(static_cast<int>(lum * (ramp.size() - 1) + 0.5f), 0,
                                     static_cast<int>(ramp.size()) - 1);
                if (opt.color && ramp[idx] != ' ') appendColor(out, false, c);
                out += ramp[idx];
            }
        }
        if (opt.color || opt.blocks) out += "\033[0m";
        out += '\n';
    }
    return out;
}
