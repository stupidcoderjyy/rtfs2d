#include <gtest/gtest.h>

#include "obstacle_geometry.h"
#include "compile/compiler_input.h"
#include "compile/compile_error.h"
#include "compile/obstacle_exp_parser.h"

namespace rtfs2d {

ObstacleGeometry geometry;
ObstacleExpLexer lexer;
ObstacleExpParser parser(geometry);

TEST(Test1, Basic) {
    auto input = CompilerInput::FromString(
        "C(1.0f, 2.0, 3) R(1, 2, 3, 4) P(0.5, 0.5f)");
    parser.Run(lexer, *input);
    ASSERT_TRUE(parser.debug_out()[0].starts_with("F1.0"));
    ASSERT_TRUE(parser.debug_out()[1].starts_with("F2.0"));
    ASSERT_TRUE(parser.debug_out()[2].starts_with("I3"));
    ASSERT_TRUE(parser.debug_out()[3].starts_with("T0"));
    ASSERT_TRUE(parser.debug_out()[4].starts_with("I1"));
    ASSERT_TRUE(parser.debug_out()[5].starts_with("I2"));
    ASSERT_TRUE(parser.debug_out()[6].starts_with("I3"));
    ASSERT_TRUE(parser.debug_out()[7].starts_with("I4"));
    ASSERT_TRUE(parser.debug_out()[8].starts_with("T1"));
    ASSERT_TRUE(parser.debug_out()[9].starts_with("F0.5"));
    ASSERT_TRUE(parser.debug_out()[10].starts_with("F0.5"));
    ASSERT_TRUE(parser.debug_out()[11].starts_with("T2"));
}

TEST(TestArgcErr, Basic) {
    auto input = CompilerInput::FromString("C(1)");
    ASSERT_THROW(parser.Run(lexer, *input), CompileError);
    input = CompilerInput::FromString("d.;");
    ASSERT_THROW(parser.Run(lexer, *input), CompileError);
}

TEST(TestSyntaxErr, Basic) {
    auto input = CompilerInput::FromString("(");
    ASSERT_THROW(parser.Run(lexer, *input), CompileError);
    input = CompilerInput::FromString("dwa");
    ASSERT_THROW(parser.Run(lexer, *input), CompileError);
    input = CompilerInput::FromString("a(1,");
    ASSERT_THROW(parser.Run(lexer, *input), CompileError);
    input = CompilerInput::FromString("a)");
    ASSERT_THROW(parser.Run(lexer, *input), CompileError);
}

TEST(TestFuncErr, Basic) {
    auto input = CompilerInput::FromString("aa(1, 2, 3)");
    ASSERT_THROW(parser.Run(lexer, *input), CompileError);
}

}