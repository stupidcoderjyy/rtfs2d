set(TARGET rtfs2d_core)

add_library(${TARGET} STATIC
        core.cpp
        core.h
)
add_library(rtfs2d::core ALIAS ${TARGET})
target_include_directories(${TARGET} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})