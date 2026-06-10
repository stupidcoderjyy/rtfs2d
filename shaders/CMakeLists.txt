set(TARGET rtfs2d_shaders)

set(SHADER_SOURCES
        advection.comp
        compute.comp
        diverge.comp
        fullscreen.frag
        fullscreen.vert
        jacobi.comp
)

find_program(GLSLC glslc HINTS "${LIB_PREFIX_PATH}/bin")

foreach(SHADER ${SHADER_SOURCES})
    set(SPV ${CMAKE_CURRENT_BINARY_DIR}/${SHADER}.spv)
    add_custom_command(
            OUTPUT ${SPV}
            COMMAND ${GLSLC} -o ${SPV} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
    )
    list(APPEND SPV_FILES ${SPV})
endforeach()

add_custom_target(${TARGET} DEPENDS ${SPV_FILES})
add_dependencies(rtfs2d ${TARGET})