set(TARGET rtfs2d_core)

add_library(${TARGET} STATIC
        window.cpp
        window.h
        grid.cpp
        grid.h
        vk_memory.cpp
        vk_memory.h
        vk_device.cpp
        vk_device.h
)
add_library(rtfs2d::core ALIAS ${TARGET})
target_include_directories(${TARGET} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${TARGET} PRIVATE
        glfw
        Vulkan::Vulkan
        spdlog::spdlog
)
