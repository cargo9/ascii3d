#include "fontzoom.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

FontZoom::FontZoom() {
    if (!std::getenv("ALACRITTY_WINDOW_ID")) return;
    FILE* p = popen("alacritty msg get-config 2>/dev/null", "r");
    if (!p) return;
    std::string json;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), p)) > 0) json.append(buf, n);
    pclose(p);

    // ищем "font":{ … "size":11.0 …}
    auto font = json.find("\"font\":");
    if (font == std::string::npos) return;
    auto size = json.find("\"size\":", font);
    if (size == std::string::npos) return;
    float s = std::strtof(json.c_str() + size + 7, nullptr);
    if (s <= 0) return;
    cur_ = s;
    ok_ = true;
}

FontZoom::~FontZoom() { restore(); }

void FontZoom::change(int steps) {
    if (!ok_ || steps == 0) return;
    float next = std::clamp(cur_ + static_cast<float>(steps), 3.0f, 48.0f);
    if (next == cur_) return;
    cur_ = next;
    changed_ = true;
    char cmd[128];
    std::snprintf(cmd, sizeof(cmd), "alacritty msg config font.size=%.1f >/dev/null 2>&1", cur_);
    if (std::system(cmd) != 0) ok_ = false;
}

void FontZoom::restore() {
    if (!changed_) return;
    changed_ = false;
    // сбрасываем только то, что меняли сами в этом окне
    if (std::system("alacritty msg config -r >/dev/null 2>&1") != 0) ok_ = false;
}
