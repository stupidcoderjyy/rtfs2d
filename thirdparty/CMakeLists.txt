set(IMGUI_TARGET imgui)

set(IMGUI_SOURCES
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_widgets.cpp
        imgui/imgui_tables.cpp
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_vulkan.cpp
)
add_library(${IMGUI_TARGET} STATIC ${IMGUI_SOURCES})
add_library(external::imgui ALIAS ${IMGUI_TARGET})
target_include_directories(${IMGUI_TARGET} PUBLIC imgui)
target_link_libraries(${IMGUI_TARGET} PRIVATE
        glfw
        Vulkan::Vulkan
)

# Imgui 示例项目
set(IMGUI_EXAMPLE_TARGET imgui_example)
add_executable(${IMGUI_EXAMPLE_TARGET}
        ${IMGUI_SOURCES}
        imgui/imgui_demo.cpp
        imgui/examples/example_glfw_vulkan/main.cpp
)
target_include_directories(${IMGUI_EXAMPLE_TARGET} PRIVATE
        imgui
        imgui/backends
)
target_link_libraries(${IMGUI_EXAMPLE_TARGET} PRIVATE
        glfw
        Vulkan::Vulkan
)

add_subdirectory(googletest)