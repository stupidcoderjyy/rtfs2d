cmake_minimum_required(VERSION 4.0)

project(rtfs2d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g")       # Debug 模式：无优化，包含调试信息
set(CMAKE_CXX_FLAGS_RELEAS "-O2 -DNDEBUG") # Release 模式：-O2 优化，定义 NDEBU

set(LIB_PREFIX_PATH "C:/msys64/mingw64")
list(PREPEND CMAKE_PREFIX_PATH ${LIB_PREFIX_PATH})

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(spdlog REQUIRED)

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE
        rtfs2d::core
        spdlog::spdlog
)

add_subdirectory(thirdparty/googletest)
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(shaders)