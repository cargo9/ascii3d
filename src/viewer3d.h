#pragma once
#include <string>

#include "mesh.h"

struct ViewOptions {
    bool color = true;
    bool once = false;   // нарисовать один кадр в stdout и выйти
    int width = 0;       // для --once; 0 = авто
    int height = 0;
};

// Интерактивный просмотр модели. Возвращает код выхода.
int runViewer(const Mesh& mesh, const ViewOptions& opt, const std::string& title);
