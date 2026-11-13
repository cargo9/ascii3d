#include "term.h"

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <cstdlib>

namespace term {
namespace {

termios g_orig{};
bool g_rawStdin = false;
bool g_active = false;
volatile std::sig_atomic_t g_quit = 0;

// alt-экран (как у vim/htop), спрятать курсор, очистить,
// включить мышь (1000 — кнопки и колесо, 1006 — понятный формат SGR)
const char kEnter[] = "\033[?1049h\033[?25l\033[2J\033[?1000h\033[?1006h";
// выключить мышь, сбросить цвет, показать курсор, вернуть обычный экран
const char kLeave[] = "\033[?1006l\033[?1000l\033[0m\033[?25h\033[?1049l";

void writeRaw(const char* data, size_t n) {
    while (n > 0) {
        ssize_t w = ::write(STDOUT_FILENO, data, n);
        if (w <= 0) return;
        data += w;
        n -= static_cast<size_t>(w);
    }
}

void restore() {
    if (!g_active) return;
    g_active = false;
    if (g_rawStdin) tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig);
    writeRaw(kLeave, sizeof(kLeave) - 1);
}

// Ctrl+C: в интерактиве просим цикл аккуратно завершиться (вернуть шрифт и т.д.),
// а если повторно или не в интерактиве — чиним терминал и выходим сразу.
void onSignal(int sig) {
    if (g_active && !g_quit) {
        g_quit = 1;
        return;
    }
    restore();
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

}  // namespace

bool isTty() { return isatty(STDOUT_FILENO); }

Size size() {
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0)
        return {ws.ws_col, ws.ws_row};
    return {80, 24};
}

void enterInteractive() {
    static bool handlersInstalled = false;
    if (!handlersInstalled) {
        std::atexit(restore);
        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);
        std::signal(SIGHUP, onSignal);
        handlersInstalled = true;
    }
    g_quit = 0;
    g_rawStdin = isatty(STDIN_FILENO);
    if (g_rawStdin) {
        tcgetattr(STDIN_FILENO, &g_orig);
        termios raw = g_orig;
        raw.c_lflag &= ~(ICANON | ECHO);  // клавиши сразу, без эха
        raw.c_cc[VMIN] = 0;               // read() не ждёт
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }
    g_active = true;
    writeRaw(kEnter, sizeof(kEnter) - 1);
}

void leaveInteractive() { restore(); }

bool quitRequested() { return g_quit != 0; }

std::vector<KeyEvent> readKeys() {
    std::vector<KeyEvent> keys;
    char buf[512];
    ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf));
    for (ssize_t i = 0; i < n; i++) {
        if (buf[i] != '\033') {
            keys.push_back({Key::Char, buf[i]});
            continue;
        }
        // мышь: ESC [ < кнопка ; x ; y M
        if (i + 2 < n && buf[i + 1] == '[' && buf[i + 2] == '<') {
            ssize_t e = i + 3;
            while (e < n && buf[e] != 'M' && buf[e] != 'm') e++;
            if (e >= n) break;  // обрывок — выкидываем
            int b = std::atoi(buf + i + 3);
            if (buf[e] == 'M' && (b & 64)) {  // 64/65 — колесо вверх/вниз
                bool ctrl = (b & 16) != 0;
                keys.push_back({(b & 1) ? Key::WheelDown : Key::WheelUp, 0, ctrl});
            }
            i = e;
            continue;
        }
        // стрелки: ESC [ A/B/C/D (или ESC O A…), с модификаторами ESC [ 1 ; 5 A
        if (i + 2 < n && (buf[i + 1] == '[' || buf[i + 1] == 'O')) {
            ssize_t e = i + 2;
            while (e < n && (buf[e] == ';' || (buf[e] >= '0' && buf[e] <= '9'))) e++;
            if (e >= n) break;
            char c = buf[e];
            if (c == 'A') keys.push_back({Key::Up});
            else if (c == 'B') keys.push_back({Key::Down});
            else if (c == 'C') keys.push_back({Key::Right});
            else if (c == 'D') keys.push_back({Key::Left});
            i = e;
            continue;
        }
        keys.push_back({Key::Escape});
    }
    return keys;
}

void write(const std::string& s) { writeRaw(s.data(), s.size()); }

std::string statusLine(const std::string& text, int cols) {
    std::string out = "\033[7m";
    int shown = 0;
    for (size_t i = 0; i < text.size() && shown < cols - 1; i++) {
        out += text[i];
        // считаем символы, а не байты (UTF-8: продолжения вида 10xxxxxx)
        if (i + 1 >= text.size() || (static_cast<unsigned char>(text[i + 1]) & 0xC0) != 0x80) shown++;
    }
    out.append(static_cast<size_t>(std::max(0, cols - 1 - shown)), ' ');
    out += "\033[0m";
    return out;
}

}  // namespace term
