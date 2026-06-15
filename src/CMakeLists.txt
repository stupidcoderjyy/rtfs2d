set(TARGET rtfs2d_core)

add_library(${TARGET} STATIC
        fluid_solvers.cpp
        grid.cpp
        obstacle_geometry.cpp
        vk_boundary.cpp
        vk_compute.cpp
        vk_descriptor.cpp
        vk_compute.cpp
        vk_device.cpp
        vk_graphics.cpp
        vk_memory.cpp
        vk_swapchain.cpp
        window.cpp
        compute_pipeline_factory.cpp
)
add_library(rtfs2d::core ALIAS ${TARGET})
target_include_directories(${TARGET} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${TARGET} PRIVATE
        glfw
        Vulkan::Vulkan
        spdlog::spdlog
)
