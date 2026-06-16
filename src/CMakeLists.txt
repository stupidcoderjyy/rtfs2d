set(TARGET rtfs2d_core)

add_library(${TARGET} STATIC
        fluid_solvers.cpp
        obstacle_geometry.cpp
        boundary_conditions.cpp
        compute_context.cpp
        compute_context.cpp
        device_manager.cpp
        graphics_context.cpp
        vk_memory.cpp
        swapchain_context.cpp
        window.cpp
        compute_pipeline_factory.cpp
        buffers.cpp
        descriptor_sets.cpp
        case_data.cpp
        obstacle_exp_parser.cpp
        imgui_vulkan_context.cpp
        util/string_input.cpp
        util/string_byte_reader.cpp
        util/buffered_input.cpp
        util/abstract_input.cpp
        compile/compiler_input.cpp
        compile/compile_error.cpp
        compile/compiler.cpp
)
add_library(rtfs2d::core ALIAS ${TARGET})
target_include_directories(${TARGET} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${TARGET} PRIVATE
        glfw
        Vulkan::Vulkan
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        external::imgui
)
