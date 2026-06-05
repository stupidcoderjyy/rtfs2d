//
// Created by PC on 2026/6/5.
//

#ifndef RTFS2D_WINDOW_H
#define RTFS2D_WINDOW_H
#include <cstdint>
#include <string>
#include <GLFW/glfw3.h>

namespace rtfs2d {

class Window {
public:
    Window(int width, int height, std::string title);
    void Show();
private:
    int width_, height_;
    std::string title_;
    GLFWwindow* window_{};
};

}

#endif //RTFS2D_WINDOW_H
