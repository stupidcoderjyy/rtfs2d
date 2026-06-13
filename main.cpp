
#include "window.h"
#include <spdlog/spdlog.h>

int main() {
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
    spdlog::set_level(spdlog::level::debug);
    rtfs2d::Window w(512, 512, "rtfs2d");
    w.Show();
    return 0;
}
