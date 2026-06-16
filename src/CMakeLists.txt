set(TARGET rtfs2d_core)

add_library(${TARGET} STATIC
        render/graphics_context.cpp
        render/imgui_vulkan_context.cpp
        render/window.cpp
        vulkan/buffers.cpp
        vulkan/compute_pipeline_factory.cpp
        vulkan/descriptor_sets.cpp
        vulkan/device_manager.cpp
        vulkan/swapchain_context.cpp
        vulkan/vk_memory.cpp
        solver/boundary_conditions.cpp
        solver/case_data.cpp
        solver/compute_context.cpp
        solver/fluid_solvers.cpp
        solver/obstacle_exp_parser.cpp
        solver/obstacle_geometry.cpp
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
