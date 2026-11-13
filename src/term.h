#pragma once
#include <string>
#include <vector>

// Работа с терминалом: размер, «сырой» режим клавиатуры и мыши, вывод.
namespace term {

struct Size {
    int cols;
    int rows;
};

enum class Key { Up, Down, Left, Right, Escape, Char, WheelUp, WheelDown };

struct KeyEvent {
    Key key;
    char ch = 0;        // для Key::Char
    bool ctrl = false;  // для колеса: зажат Ctrl
};

bool isTty();                      // stdout — это терминал (а не файл/pipe)?
Size size();                       // размер окна, по умолчанию 80×24
void enterInteractive();           // alt-экран, без курсора, клавиши без Enter, мышь
void leaveInteractive();           // вернуть всё как было
std::vector<KeyEvent> readKeys();  // неблокирующее чтение нажатий и колеса
bool quitRequested();              // пришёл Ctrl+C / SIGTERM во время интерактива
void write(const std::string& s);

// Нижняя строка-подсказка в инверсных цветах, обрезанная/дополненная до cols
std::string statusLine(const std::string& text, int cols);

}  // namespace term
