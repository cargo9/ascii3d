// Полноэкранный просмотр картинки с Ctrl+колесом (размер шрифта = детализация)
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>

#include "fontzoom.h"
#include "image.h"
#include "term.h"

int runImageViewer(const Image& img, ImageOptions opt, const std::string& title) {
    opt.width = 0;  // в полноэкранном режиме всегда вписываем в окно
    FontZoom font;
    term::enterInteractive();

    term::Size last{0, 0};
    bool dirty = true;
    bool running = true;
    while (running && !term::quitRequested()) {
        int fontSteps = 0;
        for (auto k : term::readKeys()) {
            using term::Key;
            char ch = static_cast<char>(std::tolower(static_cast<unsigned char>(k.ch)));
            if (k.key == Key::Escape || (k.key == Key::Char && ch == 'q')) running = false;
            else if (k.key == Key::WheelUp && k.ctrl) fontSteps++;
            else if (k.key == Key::WheelDown && k.ctrl) fontSteps--;
            else if (ch == '+' || ch == '=') fontSteps++;
            else if (ch == '-' || ch == '_') fontSteps--;
            else if (ch == 'b') { opt.blocks = !opt.blocks; dirty = true; }
            else if (ch == 'c') { opt.color = !opt.color; dirty = true; }
            else if (ch == 'i') { opt.invert = !opt.invert; dirty = true; }
        }
        font.change(fontSteps);

        term::Size ts = term::size();
        if (ts.cols != last.cols || ts.rows != last.rows) {
            last = ts;
            dirty = true;
        }
        if (dirty) {
            dirty = false;
            int cols = 0, rows = 0;
            std::string pic = renderImage(img, opt, ts.cols, std::max(1, ts.rows - 1), &cols, &rows);
            int left = std::max(0, (ts.cols - cols) / 2);
            int top = std::max(0, (ts.rows - 1 - rows) / 2);

            // каждую строку ставим курсором — так картинка по центру
            std::string frame = "\033[2J";
            size_t pos = 0;
            for (int y = 0; y < rows && pos < pic.size(); y++) {
                size_t end = pic.find('\n', pos);
                if (end == std::string::npos) end = pic.size();
                char move[32];
                std::snprintf(move, sizeof(move), "\033[%d;%dH", top + y + 1, left + 1);
                frame += move;
                frame.append(pic, pos, end - pos);
                pos = end + 1;
            }
            char info[320];
            std::snprintf(info, sizeof(info), " %s · %d×%d px → %d×%d символов%s · b блоки · c цвет · i инверсия · q выход",
                          title.c_str(), img.w, img.h, cols, rows,
                          font.available() ? " · Ctrl+колесо или +/- размер символов" : "");
            char move[32];
            std::snprintf(move, sizeof(move), "\033[%d;1H", ts.rows);
            frame += move;
            frame += term::statusLine(info, ts.cols);
            term::write(frame);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    font.restore();
    term::leaveInteractive();
    return 0;
}
