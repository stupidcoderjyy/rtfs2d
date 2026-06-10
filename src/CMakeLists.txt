set(TARGET rtfs2d_core)

add_library(${TARGET} STATIC
        advection_solver.cpp
        advection_solver.h
        grid.cpp
        grid.h
        jacobi_solver.cpp
        jacobi_solver.h
        vk_compute.cpp
        vk_compute.h
        vk_descriptor.cpp
        vk_compute.cpp
        vk_compute.h
        vk_descriptor.h
        vk_device.cpp
        vk_device.h
        vk_graphics.cpp
        vk_graphics.h
        vk_memory.cpp
        vk_memory.h
        vk_swapchain.cpp
        vk_swapchain.h
        window.cpp
        window.h
)
add_library(rtfs2d::core ALIAS ${TARGET})
target_include_directories(${TARGET} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${TARGET} PRIVATE
        glfw
        Vulkan::Vulkan
        spdlog::spdlog
)
