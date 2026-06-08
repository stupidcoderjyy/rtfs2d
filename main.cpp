
#include "window.h"
#include <spdlog/spdlog.h>

int main() {
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
    rtfs2d::Window w(800, 600, "rtfs2d");
    w.Show();
    return 0;
}
