#pragma once

// Размер шрифта терминала «как в браузере» — через IPC Alacritty
// (alacritty msg config font.size=…). В других терминалах просто недоступно.
class FontZoom {
public:
    FontZoom();
    ~FontZoom();                   // вернуть шрифт как был
    bool available() const { return ok_; }
    void change(int steps);        // +1 крупнее, -1 мельче
    void restore();
    float size() const { return cur_; }

private:
    bool ok_ = false;
    bool changed_ = false;
    float cur_ = 11.0f;
};
