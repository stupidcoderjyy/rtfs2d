enable_testing()
include(GoogleTest)

set(TARGET rtfs2d_test)

add_executable(${TARGET}
        test_obs_compiler.cpp
)

target_link_libraries(${TARGET} PRIVATE
        rtfs2d::core
        GTest::gtest_main
)

gtest_discover_tests(${TARGET})
