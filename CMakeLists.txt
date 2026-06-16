cmake_minimum_required(VERSION 4.0)

project(rtfs2d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEAS "-O2 -DNDEBUG")

set(LIB_PREFIX_PATH "C:/msys64/mingw64")
list(PREPEND CMAKE_PREFIX_PATH ${LIB_PREFIX_PATH})

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE
        rtfs2d::core
        spdlog::spdlog
)

# 构建后复制示例 JSON 到二进制目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/examples/test.json"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/test.json"
    COMMENT "Copying test.json to binary directory"
)

# 启动参数（支持 CLion / VS 调试器）
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_COMMAND_ARGUMENTS "test.json"
)

add_subdirectory(thirdparty/googletest)
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(shaders)
